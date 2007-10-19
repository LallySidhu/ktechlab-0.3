/***************************************************************************
 *   Copyright (C) 2003-2004 by David Saxton                               *
 *   david@bluehaze.org                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "bjt.h"
#include "circuit.h"
#include "elementset.h"
#include "element.h"
#include "logic.h"
#include "matrix.h"
#include "nonlinear.h"

#include <kdebug.h>

#include <cmath>
#include <iostream>
#include <cassert>

ElementSet::ElementSet(Circuit *circuit, const int n, const int m)
	: p_logicIn(0), m_pCircuit(circuit)
{
// Matrix class will abort if given 0 as parameter.
	int matsize = m + n;
	if(matsize) {
		p_A = new Matrix(n, matsize);
		p_b = new QuickVector(matsize);
		p_x = new QuickVector(matsize);
	} else {
		p_A = 0;
		p_x = p_b = 0;
	}

/*
	m_cn = n;
	m_cnodes = new CNode*[m_cn];
	for( uint i=0; i<m_cn; i++) {
		m_cnodes[i] = new CNode(i);
	}
*/

	m_cnodes = new CNodeList_T(n);
	m_cbranches = new CBranchList_T(m);

	m_ground = new CNode();
	m_ground->isGround = true;

	goodToGo = false;
	b_containsNonLinear = false;
}

ElementSet::~ElementSet()
{
	const ElementList::iterator end = m_elementList.end();
	for( ElementList::iterator it = m_elementList.begin(); it != end; ++it) {
		// Note: By calling setElementSet(0), we might have deleted it (the Element will commit
		// suicide when both the the ElementSet and Component to which it belongs have deleted
		// themselves). So be very careful it you plan to do anything with the (*it) pointer
		if(*it) (*it)->elementSetDeleted();
	}

/*
	for( uint i=0; i<m_cn; i++) {
		delete m_cnodes[i];
	}
	delete[] m_cnodes;
*/

	delete m_cnodes;
	delete m_cbranches;

	delete[] p_logicIn;

	delete m_ground;
	if(p_A) delete p_A;
	if(p_b) delete p_b;
	if(p_x) delete p_x;
}

void ElementSet::setCacheInvalidated()
{
	m_pCircuit->setCacheInvalidated();
}

void ElementSet::addElement(Element *e)
{
	if(!e || m_elementList.contains(e)) return;
	e->setElementSet(this);
	assert(e->type()!= Element::Element_abstract); // did someone manage to instance an abstract class?
	m_elementList.append(e);

	if(e->isNonLinear()) {
		b_containsNonLinear = true;
//		m_cnonLinearList.append(static_cast<NonLinear * >(e));
		NonLinear *foo = (NonLinear *)(&*e);

		m_cnonLinearList.append(foo);
	}
}

void ElementSet::createMatrixMap()
{
//	p_A->createMap();

	// And do our logic as well...
	m_clogic = 0;
	if(p_logicIn) delete p_logicIn;
	p_logicIn = new LogicIn*[m_clogic];

	ElementList::iterator end = m_elementList.end();
	for(ElementList::iterator it = m_elementList.begin(); it != end; ++it) {
		if((*it)->type() == Element::Element_LogicIn) {
			LogicIn *in = dynamic_cast<LogicIn*>(&(*(*it)));

			p_logicIn[m_clogic++] = in;
		}
	}
}

#define CHECK
//#define STRICT

void ElementSet::doNonLinear(int maxIterations, double maxErrorV, double maxErrorI)
{
	// And now tell the cnodes and cbranches about their new voltages & currents
	updateInfo();

	if(!p_A || !goodToGo) return;

	const NonLinearList::iterator end = m_cnonLinearList.end();
	QuickVector *p_x_prev = 0;
	int k = 0;
	bool converged;
	do {
		// Tell the nonlinear elements to update its J, A and b from the newly calculated x
		for(NonLinearList::iterator it = m_cnonLinearList.begin(); it != end; ++it)
			(*it)->update_dc();

		if(!p_A->performLU()) {
			goodToGo = false;
			return;
		}

		delete p_x;
		p_x = p_A->fbSub(p_b);

#ifdef CHECK
{
QuickVector *result = new QuickVector(p_x->size());
p_A->multiply(p_x,result);
QuickVector *error = *p_b - result;
delete result;
const unsigned int end = error->size();
double sum = 0;
for(unsigned int i = 0; i < end; i++) sum += fabs((*error)[i]);
if(sum > EPSILON) {
kdDebug() << "x then b" << endl;
p_x->dumpToAux();
p_b->dumpToAux();
kdDebug() << "Error vector:" << endl;
error->dumpToAux();
kdDebug() << sum << " FOO! check" << endl;
#ifdef STRICT
assert(false);
#endif
}
delete error;
}
#endif
		updateInfo();

		if(p_x_prev) {
		// Now, check for convergence
// note: this code is a bit slicker that what was here but it doesn't honor
// maxerrorI/V correctly. 
			double sum = 0;
			unsigned lim = m_cnodes->size() + m_cbranches->size();
			for( unsigned i = 0; i < lim; ++i) {
				sum += fabs( (*p_x_prev)[i] - (*p_x)[i]);
			}

			converged = (sum < EPSILON);
			delete p_x_prev;
		} else converged = false;

		p_x_prev = new QuickVector(p_x);

	} while(!converged && (++k < maxIterations));

if(!converged)
kdWarning() << "nonlinear calculation didn't converge" << endl;

	if(p_x_prev) delete p_x_prev;
}

// ######################

bool ElementSet::doLinear(bool performLU) {
	// check everything.
	{
	ElementList::iterator end = m_elementList.end();
	for(ElementList::iterator it = m_elementList.begin(); it != end; ++it) {
		if(!(*it)->getStatus()) {
kdWarning() << "ElementSet not ready for processing" << endl;
			return goodToGo = false;
		}
	}
	}

	if(!p_A) return false;
	if(b_containsNonLinear || (!p_b->isChanged() && ((performLU && !p_A->isChanged()) || !performLU))) return false;
	
	if(performLU) {
		if(!p_A->performLU()) {
			return goodToGo = false;
		}
	}

// copy the new vector into the temporary vector...

	delete p_x;
	p_x = p_A->fbSub(p_b);

#ifdef CHECK
{QuickVector *result = new QuickVector(p_x->size());
p_A->multiply(p_x,result);
QuickVector *error = *p_b - result;
delete result;
const unsigned int end = error->size();
double sum = 0;
for(unsigned int i = 0; i < end; i++)sum += fabs((*error)[i]);
if( sum > EPSILON) {
p_x->dumpToAux();
p_b->dumpToAux();
kdDebug() << "Error vector:" << endl;
error->dumpToAux();
kdDebug() << sum << " BAR -- check" << endl;
#ifdef STRICT
assert(false);
#endif
}
delete error;
}
#endif
	updateInfo();
	p_b->setUnchanged();

	return true;
}

void ElementSet::updateInfo()
{
	uint m_cn = m_cnodes->size();
	uint lim;
	for(uint i=0; i<m_cn; i++) {
		const double v = (*p_x)[i];
		if(std::isfinite(v)) {
			(*m_cnodes)[i].v = v;
		} else {
			(*p_x)[i] = 0.;
			(*m_cnodes)[i].v = 0.;
		}
	}

	lim = m_cbranches->size();
	for(uint i=0; i<lim; i++) {
		// NOTE: I've used lowercase and uppercase "I" here, so be careful!
		const double I = (*p_x)[i+m_cn];
		if(std::isfinite(I)) {
			(*m_cbranches)[i].i = I;
		} else {
			(*p_x)[i+m_cn] = 0.;
			(*m_cbranches)[i].i = 0.;
		}
	}

	// Tell logic to check themselves
	for(uint i=0; i<m_clogic; ++i) {
		p_logicIn[i]->check();
	}

	// check everything.
	{
	goodToGo = true;

	ElementList::iterator end = m_elementList.end();
	for(ElementList::iterator it = m_elementList.begin(); it != end; ++it) {
		if(!(*it)->getStatus()) {
			goodToGo = false;
			break;
		}
	}
	}
}

void ElementSet::displayEquations()
{
	std::cout.setf(std::ios_base::fixed);
	std::cout.precision(5);
	std::cout.setf(std::ios_base::showpoint);
	std::cout << "A x = b :"<<std::endl;

	uint lim = m_cnodes->size() + m_cbranches->size();

	for(uint i=0; i < lim; i++) {
		std::cout << "( ";
		for( uint j=0; j < lim; j++) {
			const double value = p_A->g(i,j);
// 			if( value > 0) cout <<"+";
// 			else if( value == 0) cout <<" ";
			std::cout.width(10);
			std::cout << value<<" ";
		}
		std::cout << ") ( "<<(*p_x)[i]<<") = ( ";
		std::cout<<(*p_b)[i]<<")"<<std::endl;
	}
	std::cout << "A_LU:"<<std::endl;
//	p_A->displayLU();
}

