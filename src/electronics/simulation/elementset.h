/***************************************************************************
 *   Copyright (C) 2003-2004 by David Saxton                               *
 *   david@bluehaze.org                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef ELEMENTSET_H
#define ELEMENTSET_H

#include <qguardedptr.h>
#include <qvaluelist.h>
#include <vector>

using namespace std;

class CBranch;
class Circuit;
class CNode;
class Element;
class LogicIn;
class Matrix;
class NonLinear;

#include "math/qvector.h"

typedef QValueList<Element * > ElementList;
typedef QValueList<NonLinear * > NonLinearList;

typedef vector<CBranch> CBranchList_T;
typedef vector<CNode> CNodeList_T;

/**
Steps in simulation of a set of elements:
(1) Create this class with given number of nodes "n" and voltage sources "m"
(2) Add the various elements with addElement.
(3) Call performDC()
(4) Get the nodal voltages and voltage currents with x()
(5) Repeat steps 3 and 4 if necessary for further transient analysis.

This class shouldn't be confused with the Circuit class, but considered a helper class to Circuit.
Circuit will handle the simulation of a set of components over time. This just finds the DC-operating
point of the circuit for a given set of elements.

@short Handles a set of circuit elements
@author David Saxton
*/
class ElementSet
{
public:
	/**
	 * Create a new circuit, with "n" nodes and "m" voltage sources.
	 * After creating the circuit, you must call setGround to specify
	 * the ground nodes, before adding any elements.
	 */
	ElementSet(Circuit *circuit, const int n, const int m);
	/**
	 * Destructor. Note that only the matrix and supporting data is deleted.
	 * i.e. Any elements added to the circuit will not be deleted.
	 */
	~ElementSet();
	Circuit *circuit() const { return m_pCircuit; }
	void addElement(Element * e);
	void setCacheInvalidated();
	/**
	 * Returns the matrix in use. This is created once on the creation of the ElementSet
	 * class, and deleted in the destructor, so the pointer returned will never change.
	 */
	Matrix *matrix() const { return p_A; }
	/**
	 * Returns the vector for b (i.e. the independent currents & voltages)
	 */
	QuickVector *b() const { return p_b; }
	/**
	 * Returns the vector for x (i.e. the currents & voltages at the branches and nodes)
	 */
	QuickVector *x() const { return p_x; }
	void set_x(QuickVector *x) { delete p_x; p_x = x; }
	/**
	 * @return if we have any nonlinear elements (e.g. diodes, tranaistors).
	 */
	bool containsNonLinear() const { return b_containsNonLinear; }
	/**
	 * Solves for nonlinear elements, or just does linear if it doesn't contain
	 * any nonlinear.
	 */
	void doNonLinear( int maxIterations, double maxErrorV = 1e-9, double maxErrorI = 1e-12);
	/**
	 * Solves for linear and logic elements.
	 * @returns true if anything changed
	 */
	bool doLinear( bool performLU);

	CBranch &cbranches(const unsigned i) { return (*m_cbranches)[i]; }

	CNode &cnodes(const int i) {
		if(i == -1) return *m_ground;
		return (*m_cnodes)[i]; }

//	CNode *cnodes(const unsigned i) { return m_cnodes[i]; }
	CNode *ground() const { return m_ground; }

	/**
	 * Returns the number of nodes in the circuit (excluding ground 'nodes')
	 */
//	int cnodeCount() const { return m_cn; }

	int cnodeCount() const { return m_cnodes->size(); }

	/**
	 * Returns the number of voltage sources in the circuit
	 */
	int cbranchCount() const { return m_cbranches->size(); }

	void createMatrixMap();
	/**
	 * Displays the matrix equations Ax=b and J(dx)=-r
	 */
	void displayEquations();
	/**
	 * Update the nodal voltages and branch currents from the x vector
	 */
	void updateInfo();

private:
// calc enginge stuff
	Matrix *p_A;
	QuickVector *p_x;
	QuickVector *p_b;
// end calc engine stuff. 

	ElementList m_elementList;
	NonLinearList m_cnonLinearList;

	CNodeList_T *m_cnodes;
	CBranchList_T *m_cbranches;

/*
	uint m_cn; // # cnodes
	CNode **m_cnodes; // Pointer to an array of cnodes
*/

	CNode *m_ground;

	uint m_clogic;
	LogicIn **p_logicIn;
	bool b_containsNonLinear;
	bool goodToGo;
	Circuit *m_pCircuit;
};

#endif

