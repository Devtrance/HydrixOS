/*
 *
 * dpll.c
 *
 * (C)2007 by Friedrich Gräter
 *
 * This file is distributed under the terms of
 * the GNU General Public License, Version 2. You
 * should have received a copy of this license (e.g. in 
 * the file 'copying'). 
 *
 * A simple implementation of a DPLL SAT solver
 *
 */
#include <hydrixos/types.h> 
#include <hydrixos/errno.h> 
#include <hydrixos/hymk.h> 
#include <hydrixos/tls.h>
#include <hydrixos/stdfun.h>
#include <hydrixos/blthr.h>

#include <hydrixos/system.h>
#include <hydrixos/mem.h>

#include <coredbg/cdebug.h>
  
#include "hyinit.h"  
#include "dpll.h"

/*
 * dpll_new_solver()
 *
 * Allocates a new solver data structure and initializes it.
 *
 */
dpll_solver_t* dpll_new_solver(void)
{
	dpll_solver_t *l__solver = mem_alloc(sizeof(dpll_solver_t));
	if (*tls_errno) return NULL;

	l__solver->num_variables = 0;
	l__solver->num_assignments = 0;

	l__solver->interpretation = NULL;
	l__solver->assignment_history = NULL;

	l__solver->units = NULL;
	l__solver->watchers = NULL;

	return l__solver;
}

/* 
 * dpll_new_variable(solver)
 *
 * Creates a new variable and allocates a watcher list
 * and a placeholder in the assignment history for it.
 *
 */
dpll_var_t dpll_new_variable(dpll_solver_t *solver)
{
	dpll_var_t l__var = solver->num_variables ++;
	
	solver->interpretation = mem_realloc(solver->interpretation, sizeof(int8_t) * solver->num_variables);
	if (*tls_errno) return NULL;

	solver->interpretation[l__var] = DPLL_ASSIGN_UNDEF;

	solver->watchers = mem_realloc(solver->watchers, sizeof(struct dpll_clause_st*) * solver->num_variables);
	if (*tls_errno) return NULL;

	solver->watchers[l__var] = NULL;

	return l__var;
}

/*
 * dpll_new_clause(solver, literals, num_lits)
 *
 * Creates a new clause consisting of "num_lits" literals
 * which were listed inside the array "literals". All literals
 * will be registered to the watcher list of the given solver.
 * If the clause is unit, it will be registrated as unit clause
 * and propagated when starting the solution.
 *
 * If "literals" contains the same literal twice, it will be removed.
 * If "literals" contains a tautology, the clause will be added
 * as "satisfied" to the clause database.
 *
 */
dpll_clause_t* dpll_new_clause(dpll_solver_t *solver, dpll_lit_t *literals, int num_lits)
{
	int l__i;

	if ((solver == NULL) || (literals == NULL) || (num_lits == 0))
	{
		*tls_errno = ERR_INVALID_ARGUMENT;
		return NULL;
	}

	dpll_clause_t *l__clause = mem_alloc(sizeof(dpll_clause_t));
	if (*tls_errno) return NULL;	

	l__clause->num_literals = 0;
	l__clause->free_literals = 0;
	l__clause->status = DPLL_CLSTATUS_UNDEFINED;

	l__clause->literals = mem_alloc(sizeof(dpll_lit_t) * num_lits);
	l__clause->watchers = mem_alloc(sizeof(struct dpll_clause_st*) * num_lits);

	if (*tls_errno) return NULL;

	lst_init(l__clause);

	/* Add clause to the watcher lists */
	l__i = 0;

	while(l__i < num_lits)
	{
		dpll_lit_t l__lit = literals[l__i];
		dpll_var_t l__var = dpll_var(l__lit);
		int l__ignore = 0;

		/* Search for other occurences of this variable */
		for (int l__j = 0; l__j < l__i; l__j ++)
		{
			if (l__clause->literals[l__j] == l__lit)
			{
				l__ignore = 2;
				break;
			}

			if (dpll_var(l__clause->literals[l__j]) == dpll_var(l__lit))
			{
				l__clause->status |= DPLL_CLSTATUS_SATISFIED | DPLL_CLSTATUS_TAUTOLOGY;
				l__ignore = 1;
				break;
			}
		}

		if (l__ignore < 2)
			l__clause->literals[l__i] = l__lit;
		
		if (l__ignore)
			continue;
		
		if (l__var > solver->num_variables)
		{
			*tls_errno = ERR_INVALID_ARGUMENT;
			return NULL;
		}

		l__clause->watchers[l__i] = solver->watchers[l__var];
		solver->watchers[l__var] = l__clause;

		l__i ++;
	}

	/* If clause is unit, remember it for propagation */
	if (num_lits == 1)
		lst_add(solver->units, l__clause);

	solver->num_clauses ++;
	solver->unsatisfied_clauses ++;

	return l__clause;
}

/*
 * dpll_assign_variable(solver, literal);
 *
 * Assigns a variable to a value. This function will add the
 * assignment to the current interpretation. It will also
 * analyze the watcher list of the variable. 
 *
 * If a clause in the watcher list got TRUE under this assignment the
 * count of unsatisfied clauses will be removed unless this
 * clause wasn't true before. If the count of unsatisfied 
 * clauses is 0, the function will return with DPLL_SAT, because
 * all clauses got TRUE under the current assignment.
 *
 * Also the count of free literals of this clause will be
 * decremented. If this counter reaches 0 and the clause
 * is not TRUE under the current assignment, the function
 * will return DPLL_UNSAT, becuase the clause contains
 * no literal which is TRUE under the current assignment.
 *
 * If a selected clause got unit under the current assignment,
 * and if the remaining literal is not "literal", the
 * clause will be added to the list of unit clauses and will
 * be propagated, if dpll_propagate() will be called next time.
 *
 * Return values:
 *	DPLL_UNDEF	The variable was assigned successfully
 *	DPLL_SAT	The formula got satisfied under this assignment
 *	DPLL_UNSAT	The formula got unsatisfiable under this assignment
 * 	DPLL_ERROR	Runtime error.
 *
 */
int dpll_assign_variable(dpll_solver_t *solver, dpll_lit_t literal)
{
	dpll_var_t l__var = dpll_var(literal);
	dpll_clause_t *l__clause;
	int l__value = dpll_lit_value(literal);

	if ((solver == NULL) || (l__var > solver->num_variables))
	{
		dc_printf("Invalid parameters (dpll_assign_variable): solver = 0x%X, literal = %i", solver, literal);
		return DPLL_ERROR;
	}

	/* Assign the literal */
	if (solver->interpretation[l__var] != DPLL_ASSIGN_UNDEF)
	{
		dc_printf("Variable already assigned to %i.", solver->interpretation[l__var]);
		return DPLL_ERROR;
	}
	solver->interpretation[l__var] = l__value;

	/* Analyze its watcher list */
	l__clause = solver->watchers[l__var];

	while (l__clause != NULL)
	{
		unsigned l__i = 0;
		while (    (l__i < l__clause->num_literals)
			&& (l__clause->free_literals > 0)
			&& (!l__clause->status)
		      )
		{
			if (dpll_var(l__clause->literals[l__i]) == l__var)
			{
				l__clause->free_literals --;

				if (l__value == dpll_lit_value(l__clause->literals[l__i]))
				{
					/* Clause gets TRUE under this assignment */
					solver->unsatisfied_clauses --;

					if (solver->unsatisfied_clauses == 0)
						return DPLL_SAT;

					l__clause->status |= DPLL_CLSTATUS_SATISFIED;
				}
				 else 
				{
					/* Clause gets FALSE under this assignment */

					if (l__clause->free_literals == 0) 
						return DPLL_UNSAT;

					if (l__clause->free_literals == 1)
						lst_add(solver->units, l__clause);
				}

				break;
			}
		}

		l__clause = lst_next(l__clause);
	}

	return DPLL_UNDEF;
}

/*
 * dpll_undo_assignment(solver, literal)
 *
 * Undos an assignment of a literal.
 *
 * This function will walk through the watcher list of the literal.
 * The counter of free literals of a clause got decremented. If
 * a clause got satisfied by this literal, the clause will set to
 * unsatisfied and the global counter of unsatisfied clauses will
 * be incremented. 
 *
 */
void dpll_undo_assignment(dpll_solver_t *solver, dpll_lit_t literal)
{
	dpll_var_t l__var = dpll_var(literal);
	dpll_clause_t *l__clause;
	int l__value = dpll_lit_value(literal);

	if ((solver == NULL) || (l__var > solver->num_variables))
	{
		dc_printf("Invalid parameters (dpll_undo_assignment): solver = 0x%X, literal = %i", solver, literal);
		return DPLL_ERROR;
	}

	/* Assign the literal */
	if (solver->interpretation[l__var] == DPLL_ASSIGN_UNDEF)
	{
		dc_printf("Variable was not assigned. Undo not possible.", solver->interpretation[l__var]);
		return DPLL_ERROR;
	}
	solver->interpretation[l__var] = DPLL_ASSIGN_UNDEF;

	/* Analyze its watcher list */
	l__clause = solver->watchers[l__var];

	while (l__clause != NULL)
	{
		int l__i = 0;
		while (    (l__i < l__clause->num_literals)
			&& (l__clause->free_literals > 0)
			&& (!l__clause->status)
		      )
		{
			if (dpll_var(l__clause->literals[l__i]) == l__var)
			{
				l__clause->free_literals ++;

				if (l__value == dpll_lit_value(l__clause->literals[l__i]))
				{
					/* Clause got TRUE under this assignment */
					solver->unsatisfied_clauses --;

					if (solver->unsatisfied_clauses == 0)
						return DPLL_SAT;

					l__clause->status |= DPLL_CLSTATUS_SATISFIED;
				}
				 else 
				{
					/* Clause gets FALSE under this assignment */

					if (l__clause->free_literals == 0) 
						return DPLL_UNSAT;

					if (l__clause->free_literals == 1)
						lst_add(solver->units, l__clause);
				}

				break;
			}
		}

		l__clause = lst_next(l__clause);
	}

	return DPLL_UNDEF;
}

/*
 * dpll_propagate(solver)
 *
 * Propagates all clauses of the units list of a solver.
 * If further clauses got unit by propagating, this function
 * will propagate them too.
 *
 * Return value:
 *	DPLL_UNDEF		Units were propagated successfully
 *	DPLL_SAT		Formula got satisfied during propagation
 *	DPLL_UNSAT		Conflict occured during propagation
 *	DPLL_ERROR		Runtime error.
 *
 */
int dpll_propagate(dpll_solver_t *solver)
{
	if (solver == NULL)
	{
		dc_printf("Invalid argument (dpll_propagate).\n");
		return DPLL_ERROR;
	}

	while (solver->units != NULL)
	{
		dpll_lit_t l__lit;

		for (int l__i = 0; l__i < solver->units->num_literals; l__i ++)
		{
			l__lit = solver->units->literals[l__i];

			if (solver->interpretation[l__lit] == DPLL_UNDEF)
			{
				break;
			}
		}

		solver->units = lst_next(solver->units);
		
		/* If this literal was not allready assigned by another unit clause, assign it */
		if (solver->units->status == 0)
		{
			int l__stat = dpll_assign_variable(solver, l__lit);

			if (l__stat != DPLL_UNDEF)
				return l__stat;
		}
	}

	return DPLL_UNDEF;
}

