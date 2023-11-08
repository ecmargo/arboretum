import math

# f -> percent of malicious users
# C -> size of each committee
# N -> number of committees in each query
# q -> number of queries
# def privacy_failure(f, C, N, q):
#     p = math.exp(-f*C)
#     p *= (5*math.e*f/2)**(2*C/5)
#     return "{:e}".format(min(2*p*N*q, 1))
#
# print(privacy_failure(3/100, 45, 50000, 1))


def privacy_failure(f, C, N, q):
    p = math.exp(-f*C)
    p *= (2*math.e*f)**(C/2)
    return "{:e}".format(min(2*p*N*q, 1))

# print(privacy_failure(3/100, 42, 120000, 1000))


def tfheSumExpanded(nCategories,nClients,nCores):
  basicTime = 0
  basicBandwidth = 0
  aggBandwidth = 0
  aggTime = 0

  #Aggregator Download size
  aggBandwidth += (basicBandwidth*nClients)
  total_gates = 0

  #Aggregator Verifies Proofs - Not sure if this is correct
  # aggTime += (nClients*.01)/nCores

  #Aggregator Sums
  for i in range(1,int(math.log2(nClients))+1):
      bits = 1
      if i%2==0:
        bits = i
      else:
        bits = i+1
      gates = bits*5
      # bytez = bits*2048
      nadd = (nClients)/(2**i)
      total_gates += nadd*gates*nCategories
      aggTime+= (.01*1000*nadd*gates*nCategories)
      # aggBandwidth += (bytez*nadd*nCategories)/nCores

  #return aggTime/(36e5*nCores)
  return total_gates

# print(tfheSumExpanded(1,1e9,1000))
