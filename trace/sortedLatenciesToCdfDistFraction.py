from __future__ import division 
import csv 
import sys 
from collections import defaultdict 
import numpy as np 
import matplotlib.pyplot as plt 
import glob
import collections

def main(filename=None):
    if filename is None:
        sys.stderr.write("usage: python3 %s <file-name>\n")
        return 1
    print("Trying :" + filename)
    with open(filename, 'r') as f:
	lats = f.read().splitlines()
	lats = [float(x) for x in lats]
	print("Read File")
	distri = collections.Counter(lats)
	print("Finished counting stats")
	#count = len(lats) - distri[1200.0] 
	#distri.pop(1200.0)
	#print distri
	count = len(lats)	
	for key in distri:
	    distri[key] = float(distri[key])/count
	print("Finished computing fraction")
	distri = collections.OrderedDict(sorted(distri.items()))
	#print distri	
    	print("Now writing to file")

    #Dump Latencies
    filename = filename[:-4]
    f = open(filename + "_normal.csv",'w+')
    sum = 0.0
    for k,v in  distri.iteritems():
        sum += v
        f.write(str(k) + "\t" + str(sum) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:]))

