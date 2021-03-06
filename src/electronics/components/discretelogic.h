/***************************************************************************
 *   Copyright (C) 2005 by David Saxton                                    *
 *   david@bluehaze.org                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef DISCRETELOGIC_H
#define DISCRETELOGIC_H

#include "simplecomponent.h"
#include "logic.h"

/**
@short Boolean NOT
@author David Saxton
*/
class Inverter : public CallbackClass, public SimpleComponent {

public:
	Inverter(ICNDocument *icnDocument, bool newItem, const char *id = 0);
	~Inverter();

	static Item* construct(ItemDocument *itemDocument, bool newItem, const char *id);
	static LibraryItem *libraryItem();

protected:
	void inStateChanged(bool newState);
	virtual void drawShape(QPainter &p);

	LogicIn m_pIn;
	LogicOut m_pOut;
};

/**
@short Buffer
@author David Saxton
*/
class Buffer : public CallbackClass, public SimpleComponent {

public:
	Buffer(ICNDocument *icnDocument, bool newItem, const char *id = 0);
	~Buffer();

	static Item* construct(ItemDocument *itemDocument, bool newItem, const char *id);
	static LibraryItem *libraryItem();

private:
	void inStateChanged(bool newState);
	virtual void drawShape(QPainter &p);

	LogicIn m_pIn;
	LogicOut m_pOut;
};

/**
@short Boolean logic input
@author David Saxton
*/
class ECLogicInput : public SimpleComponent {

public:
	ECLogicInput(ICNDocument *icnDocument, bool newItem, const char *id = 0);
	~ECLogicInput();

	static Item* construct(ItemDocument *itemDocument, bool newItem, const char *id);
	static LibraryItem *libraryItem();

	virtual void buttonStateChanged(const QString &id, bool state);

private:
	virtual void dataChanged();
	virtual void drawShape(QPainter &p);
	LogicOut m_pOut;
};

/**
@short Boolean logic output
@author David Saxton
*/
class ECLogicOutput : public CallbackClass, public SimpleComponent {

public:
	ECLogicOutput(ICNDocument *icnDocument, bool newItem, const char *id = 0);
	~ECLogicOutput();

	static Item* construct(ItemDocument *itemDocument, bool newItem, const char *id);
	static LibraryItem *libraryItem();

protected:
	void inStateChanged(bool newState);
	virtual void drawShape(QPainter &p);

	unsigned long long m_lastDrawTime;
	unsigned long long m_lastSwitchTime;
	unsigned long long m_highTime;
	bool m_bLastState;

	double m_lastDrawState;
	LogicIn m_pIn;
};

#endif
