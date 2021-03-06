//
// C++ Implementation: circuiticndocument
//
// Description:
//
//
// Author: Zoltan P <zoltan.padrah@gmail.com>, (C) 2008
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "circuiticndocument.h"

#include "connector.h"
#include "conrouter.h"
#include "cnitemgroup.h"
#include "ecnode.h"
#include "flowcontainer.h"
#include "item.h"
#include "junctionnode.h"
#include "nodegroup.h"

#include <kdebug.h>

#include "ktlcanvas.h"
#include "src/core/ktlconfig.h"

CircuitICNDocument::CircuitICNDocument(const QString &caption, const char *name)
		: ICNDocument(caption, name) {
}

CircuitICNDocument::~CircuitICNDocument() {
	// Go to hell, QCanvas. I'm in charge of what gets deleted.
	QCanvasItemList all = m_canvas->allItems();
	const QCanvasItemList::Iterator end = all.end();
	for (QCanvasItemList::Iterator it = all.begin(); it != end; ++it)
		(*it)->setCanvas(0);

	// Remove all items from the canvas
	selectAll();
	deleteSelection();

	// Delete anything that got through the above couple of lines
	ConnectorList connectorsToDelete = m_connectorList;
	connectorsToDelete.clear();

	const ConnectorList::iterator connectorListEnd = connectorsToDelete.end();
	for (ConnectorList::iterator it = connectorsToDelete.begin(); it != connectorListEnd; ++it)
		delete *it;

	deleteAllNodes();
}

void CircuitICNDocument::deleteAllNodes() {

	const NodeGroupList::iterator nglEnd = m_nodeGroupList.end();
	for (NodeGroupList::iterator it = m_nodeGroupList.begin(); it != nglEnd; ++it)
		delete(NodeGroup*)(*it);

	m_nodeGroupList.clear();

	m_ecNodeList.clear();
}

bool CircuitICNDocument::canConnect(QCanvasItem *qcanvasItem1, QCanvasItem *qcanvasItem2) const {
	// Rough outline of what can and can't connect:
	// * At most three connectors to a node
	// * Can't have connectors going between different levels (e.g. can't have
	//   a connector coming outside a FlowContainer from inside).
	// * Can't have more than one route between any two nodes
	// * In all connections between nodes, must have at least one input and one
	//   output node at the ends.

	// nothing special in a circuit; we can connect almost anything
	return ICNDocument::canConnect(qcanvasItem1, qcanvasItem2);
}

Connector *CircuitICNDocument::createConnector(Node *node, Connector *con, const QPoint &pos2, QPointList *pointList) {
	if (!canConnect(node, con)) return 0;

	// FIXME dynamic_cast used, fix it in Connector class
	ECNode *conStartNode = dynamic_cast<ECNode *>(con->startNode());
	ECNode *conEndNode   = dynamic_cast<ECNode *>(con->endNode());
	ECNode *ecNode       = dynamic_cast<ECNode *>(node);

	const bool usedManual = con->usesManualPoints();
	ECNode *newNode = new JunctionNode(this, 0, pos2);
	QPointList autoPoints;

	if (!pointList) {
		addAllItemConnectorPoints();
		ConRouter cr(this);
		cr.mapRoute(int(node->x()), int(node->y()), pos2.x(), pos2.y());
		autoPoints = cr.pointList();
		pointList = &autoPoints;
	}

	QValueList<QPointList> oldConPoints = con->splitConnectorPoints(pos2);
	con->hide();

	// The actual new connector
	Connector *new1 = newNode->createConnector(ecNode);
	ecNode->addConnector(new1);
	new1->setRoutePoints(*pointList, usedManual);

	// The two connectors formed from the original one when split
	Connector *new2 = newNode->createConnector(conStartNode);
	conStartNode->addConnector(new2);
	new2->setRoutePoints(*oldConPoints.at(0), usedManual);

	Connector *new3 = conEndNode->createConnector(newNode);
	newNode->addConnector(new3);
	new3->setRoutePoints(*oldConPoints.at(1), usedManual);

	// Avoid flicker: tell them to update their draw lists now
	con->updateConnectorPoints(false);
	new1->updateDrawList();
	new2->updateDrawList();
	new3->updateDrawList();

	// Now it's safe to remove the connector...
	con->removeConnector();
	flushDeleteList();

	deleteNodeGroup(conStartNode);
	deleteNodeGroup(conEndNode);
	createNodeGroup(newNode)->init();

	return new1;
}

Connector *CircuitICNDocument::createConnector(Connector *con1, Connector *con2, const QPoint &pos1, const QPoint &pos2, QPointList *pointList) {
	if (!canConnect(con1, con2)) return 0;

	const bool con1UsedManual = con1->usesManualPoints();
	const bool con2UsedManual = con2->usesManualPoints();

	QValueList<QPointList> oldCon1Points = con1->splitConnectorPoints(pos1);
	QValueList<QPointList> oldCon2Points = con2->splitConnectorPoints(pos2);

	ECNode *node1a = dynamic_cast<ECNode*>(con1->startNode());
	ECNode *node1b = dynamic_cast<ECNode*>(con1->endNode());
	ECNode *node2a = dynamic_cast<ECNode*>(con2->startNode());
	ECNode *node2b = dynamic_cast<ECNode*>(con2->endNode());

	if (!node1a || !node1b || !node2a || !node2b) return 0;

	con1->hide();
	con2->hide();

	// from this point forward, we are dealing with a circuit document -> all nodes are electronic

	ECNode *newNode1 = new JunctionNode(this, 0, pos1);
	ECNode *newNode2 = new JunctionNode(this, 0, pos2);

	Connector *con1a = newNode1->createConnector(node1a);
	node1a->addConnector(con1a);

	Connector *con1b = newNode1->createConnector(node1b);
	node1b->addConnector(con1b);

	Connector *newCon = newNode1->createConnector(newNode2);
	newNode2->addConnector(newCon);

	Connector *con2a = node2a->createConnector(newNode2);
	newNode2->addConnector(con2a);

	Connector *con2b = node2b->createConnector(newNode2);
	newNode2->addConnector(con2b);

	if (!con1a || !con1b || !con2a || !con2b) {
		// This should never happen, as the canConnect function should strictly
		// determine whether the connectors could be created before hand.
		kdWarning() << k_funcinfo << "Not all the connectors were created, this should never happen" << endl;

		if(con1a) con1a->removeConnector();
		if(con1b) con1b->removeConnector();
		if(con2a) con2a->removeConnector();
		if(con2b) con2b->removeConnector();

		newNode1->removeNode();
		newNode2->removeNode();

		flushDeleteList();

		return 0;
	}

	con1a->setRoutePoints(*oldCon1Points.at(0), con1UsedManual);
	con1b->setRoutePoints(*oldCon1Points.at(1), con1UsedManual);
	con2a->setRoutePoints(*oldCon2Points.at(0), con2UsedManual);
	con2b->setRoutePoints(*oldCon2Points.at(1), con2UsedManual);

	QPointList autoPoints;

	if (!pointList) {
		addAllItemConnectorPoints();
		ConRouter cr(this);
		cr.mapRoute(pos1.x(), pos1.y(), pos2.x(), pos2.y());
		autoPoints = cr.pointList();
		pointList = &autoPoints;
	}

	newCon->setRoutePoints(*pointList, true);

	// Avoid flicker: tell them to update their draw lists now
	con1->updateConnectorPoints(false);
	con2->updateConnectorPoints(false);
	newCon->updateDrawList();
	con1a->updateDrawList();
	con1b->updateDrawList();
	con2a->updateDrawList();
	con2b->updateDrawList();

	// Now it's safe to remove the connectors
	con1->removeConnector();
	con2->removeConnector();

	flushDeleteList();

	deleteNodeGroup(node1a);
	deleteNodeGroup(node1b);
	deleteNodeGroup(node2a);
	deleteNodeGroup(node2b);

	NodeGroup *ng = createNodeGroup(newNode1);

	ng->addNode(newNode2, true);
	ng->init();

	return newCon;
}

Connector *CircuitICNDocument::createConnector(const QString &startNodeId, const QString &endNodeId, QPointList *pointList) {

// must call this function to avoide creating fake nodes. 
	ECNode *startNode = dynamic_cast<ECNode*>(nodeWithID(startNodeId));
	ECNode *endNode   = dynamic_cast<ECNode*>(nodeWithID(endNodeId  ));

	if(!startNode || !endNode) {
		kdDebug() << "Either/both the connector start node and end node could not be found" << endl;
		return 0;
	}

	if (!canConnect(startNode, endNode)) return 0;

	Connector *connector = endNode->createConnector(startNode);

	if (!connector) {
		kdError() << k_funcinfo << "End node did not create the connector" << endl;
		return 0;
	}

	startNode->addConnector(connector);

	flushDeleteList(); // Delete any connectors that might have been removed by the nodes

	// Set the route to the manual created one if the user created such a route
	if (pointList) connector->setRoutePoints(*pointList, true);

	setModified(true);
	requestRerouteInvalidatedConnectors();

	return connector;
}

Node *CircuitICNDocument::nodeWithID(const QString &id) {

	ECNodeMap::const_iterator it = m_ecNodeList.find(id);
	if(it == m_ecNodeList.end()) return 0;
	else return it->second;
}

void CircuitICNDocument::slotAssignNodeGroups() {
	ICNDocument::slotAssignNodeGroups();

	const ECNodeMap::iterator end = m_ecNodeList.end();
	for (ECNodeMap::iterator it = m_ecNodeList.begin(); it != end; ++it) {
		NodeGroup *ng = createNodeGroup(it->second);
		if(ng) ng->init();
	}

	// We've destroyed the old node groups, so any collapsed flowcontainers
	// containing new node groups need to update them to make them invisible.
	const ItemMap::const_iterator itemListEnd = m_itemList.end();
	for (ItemMap::const_iterator it = m_itemList.begin(); it != itemListEnd; ++it) {
		if (FlowContainer *fc = dynamic_cast<FlowContainer*>(it->second))
			fc->updateContainedVisibility();
	}
}

void CircuitICNDocument::flushDeleteList() {
	// Remove duplicate items in the delete list
	QCanvasItemList::iterator end = m_itemDeleteList.end();
	for (QCanvasItemList::iterator it = m_itemDeleteList.begin(); it != end; ++it) {
		if (*it && m_itemDeleteList.contains(*it) > 1) {
			*it = 0;
		}
	}

	m_itemDeleteList.remove(0);

	/* again we're spending time to figure out what special method to call instead of a generic call..*/
	end = m_itemDeleteList.end();
	for (QCanvasItemList::iterator it = m_itemDeleteList.begin(); it != end; ++it) {
		QCanvasItem *qcanvasItem = *it;
		m_selectList->removeQCanvasItem(*it);

		if (Item *item = dynamic_cast<Item*>(qcanvasItem))
			m_itemList.erase(item->id());
		else if (ECNode *node = dynamic_cast<ECNode*>(qcanvasItem))
			m_ecNodeList.erase(node->id());
		else if (Connector *con = dynamic_cast<Connector*>(qcanvasItem))
			m_connectorList.erase(con);
		else	kdError() << k_funcinfo << "Unknown qcanvasItem! " << qcanvasItem << endl;

		qcanvasItem->setCanvas(0);

		delete qcanvasItem;
	}

	m_itemDeleteList.clear();

	// Check connectors for merging
	bool doneJoin = false;

	const ECNodeMap::iterator nlEnd = m_ecNodeList.end();
	for (ECNodeMap::iterator it = m_ecNodeList.begin(); it != nlEnd; ++it) {
		assert(it->second);

		int conCount = it->second->getAllConnectors().size();
		if(conCount == 2 && !it->second->parentItem()) {
			if (joinConnectors(it->second))
				doneJoin = true;
		}
	}

	if (doneJoin) flushDeleteList();

	requestRerouteInvalidatedConnectors();
}

bool CircuitICNDocument::registerItem(QCanvasItem *qcanvasItem) {
	if (!qcanvasItem) return false;

	if (!ItemDocument::registerItem(qcanvasItem)) {
		if(ECNode *node = dynamic_cast<ECNode*>(qcanvasItem)) {
			m_ecNodeList.insert(std::pair< QString, ECNode* >(node->id(), node));
			emit nodeAdded((Node*)node);
		} else if(Connector *connector = dynamic_cast<Connector*>(qcanvasItem)) {
			m_connectorList.insert(connector);
			emit connectorAdded(connector);
		} else {
			kdError() << k_funcinfo << "Unrecognised item" << endl;
			return false;
		}
	}

	requestRerouteInvalidatedConnectors();

	return true;
}

void CircuitICNDocument::unregisterUID(const QString &uid) {
	m_ecNodeList.erase(uid);
	ICNDocument::unregisterUID(uid);
}

/* this method smells a little funny */
NodeList CircuitICNDocument::nodeList() const {
	NodeList l;

	ECNodeMap::const_iterator end = m_ecNodeList.end();
	for (ECNodeMap::const_iterator it = m_ecNodeList.begin(); it != end; ++it) {
assert(it->second);
		l << it->second;
	}

	return l;
}

void CircuitICNDocument::selectAllNodes() {
	const ECNodeMap::iterator nodeEnd = m_ecNodeList.end();
	for (ECNodeMap::iterator nodeIt = m_ecNodeList.begin(); nodeIt != nodeEnd; ++nodeIt) {
assert(nodeIt->second);
		select(nodeIt->second);
	}
}

bool CircuitICNDocument::joinConnectors(ECNode *node) {
	// We don't want to destroy the node if it has a parent
	if (node->parentItem()) return false;

	// an electronic node can be removed if it has exactly 2 connectors connected to it
	int conCount = node->getAllConnectors().size();
	if (conCount != 2) return false;

	Connector *con1, *con2;
	ECNode *startNode, *endNode;
	QPointList conPoints;

	{
		std::set<Connector *>::iterator it = node->getAllConnectors().begin();

		con1 = *it;
		it++;
		con2 = *it;
	}

	if (con1 == con2) return false;

	// we don't know on which end of the connectors is our node, so we must check both ends
	// HACK // TODO // dynamic_cast used, because Connector doesn't know about ECNode, only Node
	if (con1->startNode() == node)
		startNode = dynamic_cast<ECNode*>(con1->endNode());
	else	startNode = dynamic_cast<ECNode*>(con1->startNode());

	if (con2->startNode() == node)
		endNode = dynamic_cast<ECNode*>(con2->endNode());
	else	endNode = dynamic_cast<ECNode*>(con2->startNode());

	conPoints = con1->connectorPoints(false) + con2->connectorPoints(false);

	if (!startNode || !endNode) return false;

	Connector *newCon = endNode->createConnector(startNode);

	if (!newCon) return false;

	startNode->addConnector(newCon);
	newCon->setRoutePoints(conPoints, con1->usesManualPoints() || con2->usesManualPoints());

	// Avoid flicker: update draw lists now
	con1->updateConnectorPoints(false);
	con2->updateConnectorPoints(false);

	newCon->updateDrawList();
	node->removeNode();

	con1->removeConnector();
	con2->removeConnector();

	return true;
}

/*!
    \fn CircuitICNDocument::loadDisplayConfig()
 */
void CircuitICNDocument::loadDisplayConfig()
{
	ECNodeMap::iterator nodeEnd = m_ecNodeList.end();
	for (ECNodeMap::iterator it = m_ecNodeList.begin(); it != nodeEnd; ++it) {
		ECNode *n = it->second;
		n->setShowVoltageBars(KTLConfig::showVoltageBars());
		n->setShowVoltageColor(KTLConfig::showVoltageColor());
	}
}

/*!
    \fn CircuitICNDocument::setNodesChanged()
 */
void CircuitICNDocument::setNodesChanged()
{
	ECNodeMap::iterator end = m_ecNodeList.end();
	for (ECNodeMap::iterator it = m_ecNodeList.begin(); it != end; ++it) {
		it->second->setNodeChanged();
	}
}

#include "circuiticndocument.moc"

