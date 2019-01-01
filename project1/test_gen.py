import random

bitstring = ""

for i in range(2):
	j = i+33;

	print("# "+str(j)+":")
	print("t "+str(j))
	
	size = random.randint(9900,10100)

	for it in range(size):
		bitstring += str(random.randint(0,1))

	print("n "+bitstring)
	
	r1 = random.randint(0,125) # chose starting index
	r2 = random.randint(9000,size-1) # chose the ending index
	rot = random.randint(0,500)
	
	print("r "+ str(r1)+" "+ str(r2-r1+1)+ " "+ str(rot))
	
	m = bitstring[r1:r2+1]	

	m2 = m[len(m) - (rot%len(m)) : ] + m[ : len(m)- (rot%len(m)) ] 

	ans = bitstring[0:r1] + m2  + bitstring[r2+1:]	

	print("e "+str(ans))

	print(" ")

