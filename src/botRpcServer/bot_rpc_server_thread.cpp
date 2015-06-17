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

#include <yarp/os/Bottle.h>

#include "bot_rpc_server_thread.h"

using namespace std;
using namespace yarp::os;

BotRpcServerThread::BotRpcServerThread(RpcServer *rpc_port, string *default_answer,  Property *dictionary): rpc_port_(rpc_port), default_answer_(default_answer), dictionary_(dictionary)
{
}

void BotRpcServerThread::run()
{
	while (!isStopping())
	{
		cout << "Waiting for a message..." << endl;
		Bottle request, reply;
		rpc_port_->read(request, true);
		cout << "Message: " << request.toString() << endl;
		if (dictionary_ != NULL)
		{
			Value answer = dictionary_->find(request.toString());
			if (answer.isNull())
				reply.addString(default_answer_->c_str());
			else
				reply.addString(answer.asString());
		}
		cout << "Reply: >>" << reply.toString() << endl;
		rpc_port_->reply(reply);
	}
}
