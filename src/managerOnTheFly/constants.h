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
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <yarp/os/Vocab.h>

#define                 ACK                 VOCAB3('a','c','k')
#define                 NACK                VOCAB4('n','a','c','k')

#define                 STATE_IDLE          0
#define                 STATE_TRAINING      1
#define                 STATE_CLASSIFY      2
#define                 STATE_OBSERVING     3

#define                 MODE_ROBOT          0
#define                 MODE_HUMAN_IDLE     1
#define                 MODE_HUMAN_TRACK    2

#define                 CMD_IDLE            VOCAB4('i','d','l','e')

#define                 CMD_OBSERVE         VOCAB4('o','b','s','e')
#define                 CMD_TRAIN           VOCAB4('t','r','a','i')
#define                 CMD_CLASSIFY        VOCAB4('c','l','a','s')

#define                 CMD_ROBOT           VOCAB4('r','o','b','o')
#define                 CMD_HUMAN           VOCAB4('h','u','m','a')

#define                 CMD_FORGET          VOCAB4('f','o','r','g')

#endif
