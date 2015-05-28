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

#include <yarp/os/Time.h>

#include "manager_module.h"

ManagerModule::ManagerModule()
{}

bool 	ManagerModule::configure(ResourceFinder &rf)
{
    string name=rf.find("name").asString().c_str();

    Time::turboBoost();

    manager_thr=new ManagerThread(rf);
    manager_thr->start();

    port_rpc_human.open(("/"+name+"/human:io").c_str());
    port_rpc.open(("/"+name+"/rpc").c_str());
    attach(port_rpc);
    return true;
}

bool 	ManagerModule::interruptModule()
{
    port_rpc_human.interrupt();
    port_rpc.interrupt();
    cout << "calling manager thread interrupt..." << endl;
    manager_thr->interrupt();

    cout << "returning manager module interrupt..." << endl;
    return true;
}

bool 	ManagerModule::close()
{
    cout << "calling manager thread stop..." << endl;
    manager_thr->stop();
    delete manager_thr;

    port_rpc_human.close();
    port_rpc.close();

    cout << "returning manager module close..." << endl;
    return true;
}

bool 	ManagerModule::respond(const Bottle &command, Bottle &reply)
{
    if(manager_thr->execReq(command,reply))
        return true;
    else
        return RFModule::respond(command,reply);
}

double 	ManagerModule::getPeriod()    { return 1.0;  }

bool   	ManagerModule::updateModule()
{
    Bottle human_cmd,reply;
    port_rpc_human.read(human_cmd,true);
    if(human_cmd.size()>0)
    {
        manager_thr->execHumanCmd(human_cmd,reply);
        port_rpc_human.reply(reply);
    }

    return true;
}
