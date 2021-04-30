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
    servers = {};
    totalChoices=0; 
    wrongChoices = 0;
    ranks={};
    if filepath is None and maxSuccess is None:
        sys.stderr.write("usage: python3 %s <file-path> <expected-success-number>\n")
        return 1
    xmax = 250
    cumulatedFullLatencies = []
    filesToProcess = glob.glob(filepath+"*" )
    print filesToProcess
    for filename in filesToProcess:
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
                
                if row['event'] == 'update':
                   servers[row['server']]=row['util']
                elif (row['event'] == 'choice'): #and (row['(hopCount)/seq'] == '6'):
                   
                    rank = 1;
                    availServers = servers.copy()
                    totalChoices = totalChoices+1
                    while len(availServers) != 0:
                        bestServer = ""
                        lowestUtil = 1000;
                        for server in availServers:
                            if int(servers[server]) < lowestUtil:
                                lowestUtil = int(servers[server])
                                bestServer = server
                        #print(servers)
                        #print(bestServer)
                        #print(row['server'])
                        #print("Rank:" + str(rank))
                        if bestServer == row['server']:
                            #print("Shouldnt be here") 
                            if rank not in ranks:
                                ranks[rank] = 0 
                            ranks[rank] += 1
                            availServers.clear()
                        else:
                            rank += 1
                            availServers.pop(bestServer)
            c = 0


    #Dump Latencies
    f = open(traceID+"ChoicePercentage.csv","w")
    #f.write("Rank\tPercentage\n" )
    for rank in ranks:
        print(str(rank)+" "+str(ranks[rank]))
        f.write("\""+str(rank)+"\"\t"+str(ranks[rank]/totalChoices)+"\n" )

    return 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:]))

