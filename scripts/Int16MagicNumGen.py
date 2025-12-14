import random

def extended_gcd(a, b):
    if a == 0: return b, 0, 1
    else: g, y, x = extended_gcd(b % a, a); return g, x - (b // a) * y, y

def modinv(a, m):
    g, x, y = extended_gcd(a, m)
    if g != 1: raise Exception('Modular inverse does not exist')
    else: return x % m


MOD = 65536 


while True:
    A = random.randint(100, 60000)
    if A % 2 != 0: break


C = modinv(A, MOD)


B = random.randint(100, 60000)


D = (-(B * C)) % MOD

print(f"生成的常数: A={A}, B={B}, C={C}, D={D}")
print(f"验证公式: (({A} * x + {B}) * {C} + {D}) & 0xFFFF == x")