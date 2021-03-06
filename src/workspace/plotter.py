import sys
import os.path
import subprocess
from collections import Counter
from collections import defaultdict
import matplotlib.pyplot as plt
import copy
import numpy as np                                                               

if os.path.isfile("../output/l1hit.txt") and os.path.isfile("../output/l2hit.txt") and os.path.isfile("../output/l2miss.txt"):
	pass
else:
	for i in range(100):
		p1 = subprocess.Popen( ["./getTime2", "0" ], stderr=subprocess.STDOUT, stdout=subprocess.PIPE)
d1={};d2={};d2m={}
nd1={};nd2={};nd2m={}
d1 = defaultdict(lambda: 0, d1)
d2 = defaultdict(lambda: 0, d2)
d2m = defaultdict(lambda: 0, d2m)
with open('../output/l1hit.txt') as f:
	array1 = []
	for line in f: # read rest of lines
		array1.append([int(x) for x in line.split()])
# for i in range(100):
	# array1[i].pop(0)	
for i in range(100):
	for j in range(100):
		d1[array1[i][j]] += 1
with open('../output/l2hit.txt') as f:
	array2 = []
	for line in f: # read rest of lines
		array2.append([int(x) for x in line.split()])
# for i in range(100):
	# array2[i].pop(0)	
for i in range(100):
	for j in range(100):
		d2[array2[i][j]] += 1
with open('../output/l2miss.txt') as f:
	array2m = []
	for line in f: # read rest of lines
		array2m.append([int(x) for x in line.split()])

for i in range(100):
	for j in range(100):
		d2m[array2m[i][j]] += 1

target_index=100
nd1 = sorted(d1.items())
list_len = len(nd1)
for i in range(list_len):
	if nd1[i][0]>500:
		target_index = i 
		break
ld1 =  nd1[:target_index]
cnt1=0
for i in range(len(ld1)):
	cnt1 += ld1[i][1]

print target_index
nd2 = sorted(d2.items())
list_len = len(nd2)
for i in range(list_len):
	if nd2[i][0]>450:
		target_index = i 
		break
ld2 =  nd2[:target_index]
cnt2=0
for i in range(len(ld2)):
	cnt2 += ld2[i][1]
print target_index

nd2m = sorted(d2m.items())
list_len = len(nd2m)
for i in range(list_len):
	if nd2m[i][0]>450:
		target_index = i 
		break
ld2m =  nd2m[:target_index]
cnt2m=0
for i in range(len(ld2m)):
	cnt2m += ld2m[i][1]
# print target_index
# print cnt1	
# print cnt2	
# print cnt2m


ax = plt.subplot(111)
w = 1
interval = 20.0
for i in range(len(ld1)):
	# print i
	ax.bar(ld1[i][0],100*( float(ld1[i][1])/float(cnt1)),width=w,color='b',align='center')
for i in range(len(ld2)):
	ax.bar(ld2[i][0], 100*(float(ld2[i][1])/float(cnt2)),width=w,color='g',align='center')
for i in range(len(ld2m)):
	ax.bar(ld2m[i][0], 100*(float(ld2m[i][1])/float(cnt2m)),width=w,color='r',align='center')
plt.xticks(np.arange(20, 500, interval))

plt.show()