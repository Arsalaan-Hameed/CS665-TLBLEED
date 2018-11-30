import matplotlib.pyplot as plt
from collections import namedtuple
Pair = namedtuple("Pair", ["first", "second"])

add = {}
mul={}
tar={}
neither={}
#(Main) Target:[0x564a21547050] add:[0x564a2154704c] mul:[0x564a21547054]
target = '564a21547050'
target_mul = '564a21547054'
target_add = '564a2154704c'
file = "pinatrace.out"
read = tuple(open(file, "r"))
print len(read)
set_number = [None]*100

mul_flag=0
# for i in range(1000000):
# 	if mul_flag == 1:
# 		mul[i] = Pair(i,int((int(read[i][18:],16)>>3)%16))
# 		plt.plot(mul[i].first,mul[i].second, 'ro')
# 		# print "Set No:",int((int(read[i][18:],16)>>3)%16)
# 	if read[i][20:-1] == target_mul:
# 		print i
# 		if mul_flag == 0:
# 			mul_flag =1
# 		elif mul_flag == 1:
# 			# mul_flag=0
# 			break
# plt.axis([59333, 214344, -1, 16])
# plt.show()
fig = plt.figure()


addcnt=0
mulcnt=0
for i in range(479430):
	set_no = int((int(read[i][18:],16)>>3)%16)
	if read[i][20:-1] == target_mul:
		plt.plot(i, set_no +1, 'go')
		# print "MUL: ",i

	if read[i][20:-1] == target:
		# print "target: ",i
		plt.plot(i, set_no, 'ro')

	if read[i][20:-1] == target_add:
		# print "ADD: ",i
		plt.plot(i, set_no, 'bo')

plt.axis([59330, 479430, -1, 16])
plt.show()
print "mulcnt: ",mulcnt
print "addcnt: ",addcnt		
