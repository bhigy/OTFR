/* 
 * Copyright (C) 2015 iCub Facility - Istituto Italiano di Tecnologia
 * Author: Bertand HIGY
 * email:  bertrand.higy@iit.it
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

#include "bot_rpc_server_module.h"

using namespace std;
using namespace yarp::os;

bool BotRpcServerModule::configure(ResourceFinder &rf)
{    
	module_name_ = rf.check("name", Value("botRpcServer"), "module name (string)").asString();
	setName(module_name_.c_str());

	dictionary_config_filename_ = rf.check("dictionaryConfig", Value(""), "dictionary configuration filename (string)").asString();
	if (dictionary_config_filename_ != "")
	{
		dictionary_config_filename_ = (rf.findFile(dictionary_config_filename_.c_str())).c_str();
		if (dictionary_.fromConfigFile(dictionary_config_filename_.c_str()) == false) {
			cerr << "botRpcServerModule: unable to read dictionary configuration file" << dictionary_config_filename_;
			return false;
		}
	}

	// get the name of the input and output ports, automatically prefixing the module name by using getName()
	port_name_ = "/";
	port_name_ += getName(rf.check("port", Value("/rpc:io"), "Rpc port (string)").asString());
	 
	// open ports
	if (!rpc_port_.open(port_name_.c_str())) {
		cerr << getName() << ": unable to open port " << port_name_ << endl;
		return false;
	}
	
	default_answer_ = rf.check("defaultAnswer", Value("ack"), "Default answer (string)").asString();
   	// create the thread and pass pointers to the module parameters
	thread_ = new BotRpcServerThread(&rpc_port_, &default_answer_, &dictionary_);
	// now start the thread to do the work
	thread_->start(); // this calls threadInit() and it if returns true, it then calls run()

   	return true ;
}

bool BotRpcServerModule::updateModule()
{
   return true;
}

bool BotRpcServerModule::interruptModule()
{
   rpc_port_.interrupt();
   return true;
}

bool BotRpcServerModule::close()
{
	/* stop the thread */
	thread_->stop();

   rpc_port_.close();
   return true;
}

