/* 
 * Copyright (C) 2011 Department of Robotics Brain and Cognitive Sciences - Istituto Italiano di Tecnologia
 * Author: Carlo Ciliberto
 * email:  carlo.ciliberto@iit.it
 * Permission is granted to copy, distribute, and/or modify this program
 * under the terms of the GNU General Public License, version 2 or any
 * later version published by the Free Software Foundation.
 *
 * A copy of the license can be found at
 * http://www.robotcub.org/icub/license/gpl.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details
*/

#ifndef MANAGER_MODULE_H
#define MANAGER_MODULE_H

#include <yarp/os/Port.h>
#include <yarp/os/RFModule.h>
#include <yarp/os/RpcClient.h>
#include <yarp/os/RpcServer.h>

#include "manager_thread.h"

using namespace yarp::os;

class ManagerModule: public RFModule
{
	public:
		ManagerModule();
		bool configure(ResourceFinder &rf);
		bool interruptModule();
		bool close();
		bool respond(const Bottle &command, Bottle &reply);
		double getPeriod();
		bool   updateModule();

	protected:
		ManagerThread       *manager_thr;
		RpcServer           port_rpc_human;
		Port                port_rpc;
};

#endif
