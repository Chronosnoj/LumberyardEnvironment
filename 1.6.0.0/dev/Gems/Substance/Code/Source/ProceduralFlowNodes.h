/** @file ProceduralFlowNodes.h
	@brief Header for the Procedural Flow Graph System
	@author Josh Coyne - Allegorithmic (josh.coyne@allegorithmic.com)
	@date 09-14-2015
	@copyright Allegorithmic. All rights reserved.
*/
#ifndef GEM_SUBSTANCE_PROCEDURALFLOWNODES_H
#define GEM_SUBSTANCE_PROCEDURALFLOWNODES_H
#pragma once

#include "FlowSystem/Nodes/FlowBaseNode.h"

/// Cast a flowgraph port to a GraphInstanceID
ILINE GraphInstanceID GetPortGraphInstanceID(IFlowNode::SActivationInfo* pActInfo, int nPort)
{
	GraphInstanceID x = *(pActInfo->pInputPorts[nPort].GetPtr<GraphInstanceID>());
	return x;
}

#endif //GEM_SUBSTANCE_PROCEDURALFLOWNODES_H
