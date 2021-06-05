#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import time
import numpy as np
from ns3gym import ns3env
import random

__author__ = "Piotr Gawlowicz"
__copyright__ = "Copyright (c) 2018, Technische Universität Berlin"
__version__ = "0.1.0"
__email__ = "gawlowicz@tkn.tu-berlin.de"


parser = argparse.ArgumentParser(description='Start simulation script on/off')
parser.add_argument('--start',
                    type=int,
                    default=0,
                    help='Start simulation script 0/1, Default: 0')
parser.add_argument('--iterations',
                    type=int,
                    default=1,
                    help='Number of iterations, Default: 1')
args = parser.parse_args()
startSim = bool(args.start)
iterationNum = int(args.iterations)

port = 5555
simTime = 600 # seconds
stepTime = 10 # seconds
seed = 0
simArgs = {"--simTime": simTime,
           "--testArg": 123,
           "--distance": 500}
debug = False

env = ns3env.Ns3Env(port=port, stepTime=stepTime, startSim=startSim, simSeed=seed, simArgs=simArgs, debug=debug)
env.reset()


ob_space = env.observation_space
ac_space = env.action_space
print("Observation space: ", ob_space,  ob_space.dtype)
print("Action space: ", ac_space, ac_space.dtype)
q_table = np.zeros([10,13])*12

stepIdx = 0
currIt = 0
allRxPkts = 0

alpha = 0.1
gamma = 0.6
epsilon=0.8
def calculate_cw_window(num):
    


    k=np.random.randint(low=10,high=90, size=1)
    action = np.ones(shape=len(state), dtype=np.uint8) *10#* k[0]
    
    
    return action

try:
    while True:
        state = env.reset()
        reward = 0
        mean=np.mean(np.array(state))
        level=int(np.floor(mean/1000))
        print("Start iteration: ", currIt)
        print("Step: ", stepIdx)
        print("---state: ", state)
        print("---level: ",level)

        while True:
            stepIdx += 1

            allRxPkts += reward
            if random.uniform(0,1)<epsilon:
                action=env.action_space.sample()
            else:
                #action = [np.argmax(q_table[level])[0]+1]
                action = [np.argmax(q_table[level])]
            #action = calculate_cw_window(state)
            #action=[12]#only can take values between 7 and 12
            print("---action: ", action)

            next_state, reward, done, info = env.step(action)
            print ("distancia promedio:",np.mean(np.array(next_state)))
            mean=np.mean(np.array(next_state))
            level=int(np.floor(mean/1000))
            q_table[level,action]=q_table[level,action] if q_table[level,action]>reward else reward
            print("Step: ", stepIdx)
            print("---state, reward, done, info: ", next_state, reward, done, info)
            
            #old_value=q_table[state,action]
            #next_max = np.max(q_table[next_state])
            #new_value = (1 - alpha) * old_value + alpha * (reward + gamma * next_max)
            #q_table[state, action] = new_value

            if done:
                stepIdx = 0
                print("All rx pkts num: ", allRxPkts)
                allRxPkts = 0

                if currIt + 1 < iterationNum:
                    env.reset()
                break

        currIt += 1
        if currIt == iterationNum:
            break

except KeyboardInterrupt:
    print("Ctrl-C -> Exit")
finally:
    env.close()
    print("Done")