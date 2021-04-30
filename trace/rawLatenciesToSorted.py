from __future__ import division 
import csv 
import sys 
from collections import defaultdict 
import numpy as np 
import matplotlib.pyplot as plt 
import glob

def main(filepath=None, traceID=None, maxSuccess=None):
    count = []
    node_count = 0;
    latency_sum = []
    tp = []
    sent = []
    nodes = {};
    for i in range(0,5000):
	tp.append(0)
	count.append(0)
	latency_sum.append(0)
	sent.append(0)
    if filepath is None and maxSuccess is None:
        sys.stderr.write("usage: python3 %s <file-path> <expected-success-number>\n")
        return 1
    xmax = 250
    cumulatedFullLatencies = []
    filesToProcess = glob.glob(filepath+"*" )
    print filesToProcess
    for filename in filesToProcess:
        pending = {}
        pending1 = {}
        fullLatencies = []
        latencies = defaultdict(lambda: [])
        print("Trying :" + filename)
        with open(filename, 'r') as csvh:
            #dialect = csv.Sniffer().sniff(csvh.read(10*1024))
            dialect = csv.Sniffer().has_header(csvh.read(1024))
            csvh.seek(0)
            reader = csv.DictReader(csvh, dialect=dialect)

            for row in reader:          
                node = int(row['nodeid'])
		if node not in nodes:
			nodes[node] = 1;
			node_count += 1;
                time = float(row['time'])
                
                if row['event'] == 'sent':
                   if (row['name']) not  in pending:
                      pending[(row['name'])] = time
		   sent[int(time)] += 1
                   pending1[ row['name']] = time
                elif (row['event'] == 'received'): #and (row['(hopCount)/seq'] == '6'):
                    #assert((node, row['name']) in pending)
                    latencies[node].append(1. * (time - pending[( row['name'])]))
                    latency_sum[int(time)] += 1000. * (time - pending[( row['name'])])
                    count[int(time)] += 1
		    tp[int(pending1[( row['name'])])] += 1
                    del pending[( row['name'])]
                    #del pending1[(node, row['name'])]
                #else:
                    #del pending[(node, row['name'])]
                    #del pending1[(node, row['name'])]
            c = 0
            f = open(traceID + "UnsortedLatencies.txt","w")
            for key in latencies:
                for latencyVal in latencies[key]:
                    fullLatencies.append(latencyVal)
                    f.write(str(latencyVal) + "\n")


	    print(str(filename) + " Success rate: " + str(len(fullLatencies)))
	    maxSuccess = len(fullLatencies)
	    #maxSuccess = 1
	    #if len(fullLatencies) < int(maxSuccess):
            for x in range(int(maxSuccess) - len(fullLatencies)):
	        fullLatencies.append(xmax)

	    cumulatedFullLatencies.extend(fullLatencies)
 




    print node_count;


    for i in range(0,180):
	if(count[i]==0):
		latency_sum[i] = 0
	else:
		latency_sum[i] = latency_sum[i]/count[i]
 
    for i in range(0,180):
	if(sent[i]==0):
		tp[i] = 1
	else:
		tp[i] = tp[i]/sent[i]
   
 
    #Dump Latencies
    cumulatedFullLatencies.sort()
    f = open(traceID+"SortedLatencies.txt","w")
    for each in cumulatedFullLatencies:
        f.write(str(each) + "\n")

    return 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:]))

