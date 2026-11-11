#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>

class U256 {
private:
    uint64_t w[4];

public:
    // Конструкторы
    U256() { w[0] = w[1] = w[2] = w[3] = 0; }
    U256(uint64_t v) { w[0] = v; w[1] = w[2] = w[3] = 0; }

    // Доступ (для итераций по словам в CSWAP и сравнении)
    uint64_t get_word(int i) const { return w[i]; }
    void set_word(int i, uint64_t val) { w[i] = val; }

    // Базовые проверки
    bool is_zero() const {
        return (w[0] | w[1] | w[2] | w[3]) == 0;
    }
    bool is_one() const {
        return w[0] == 1 && w[1] == 0 && w[2] == 0 && w[3] == 0;
    }
    int bitlen() const {
        for (int i = 3; i >= 0; --i)
            if (w[i]) {
                int b = 0; uint64_t x = w[i];
                while (x) { ++b; x >>= 1; }
                return i * 64 + b;
            }
        return 0;
    }
    uint64_t get_bit(int bit) const {
        return (w[bit >> 6] >> (bit & 63)) & 1ULL;
    }

    // Статические фабрики
    static U256 from_hex(const char* s) {
        U256 r;
        size_t n = strlen(s);
        int word = 0;
        int i = (int)n - 1;
        while (i >= 0 && word < 4) {
            uint64_t acc = 0;
            int cnt = 0;
            while (i >= 0 && cnt < 16) {
                char c = s[i];
                uint64_t v;
                if (c >= '0' && c <= '9') v = c - '0';
                else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
                else { --i; continue; }
                acc |= v << (4 * cnt);
                ++cnt; --i;
            }
            r.w[word++] = acc;
        }
        return r;
    }

    void print(const char* label = nullptr) const {
        if (label) printf("%s = 0x", label);
        else printf("0x");
        for (int i = 3; i >= 0; --i) printf("%016llx", (unsigned long long)w[i]);
        printf("\n");
    }

    // Арифметические операторы
    friend U256 operator+(const U256& a, const U256& b) {
        U256 r; uint64_t carry = 0;
        for (int i = 0; i < 4; ++i) {
            uint64_t s = a.w[i] + b.w[i];
            uint64_t c1 = (s < a.w[i]) ? 1ULL : 0ULL;
            uint64_t s2 = s + carry;
            uint64_t c2 = (s2 < s) ? 1ULL : 0ULL;
            r.w[i] = s2;
            carry = c1 | c2;
        }
        return r;
    }

    friend U256 operator-(const U256& a, const U256& b) {
        U256 r; uint64_t borrow = 0;
        for (int i = 0; i < 4; ++i) {
            uint64_t d = a.w[i] - b.w[i];
            uint64_t b1 = (a.w[i] < b.w[i]) ? 1ULL : 0ULL;
            uint64_t d2 = d - borrow;
            uint64_t b2 = (d < borrow) ? 1ULL : 0ULL;
            r.w[i] = d2;
            borrow = b1 | b2;
        }
        return r;
    }

    friend U256 operator*(const U256& a, const U256& b) {
        uint64_t res[8] = {0};
        for (int i = 0; i < 4; ++i) {
            uint64_t carry = 0;
            for (int j = 0; j < 4; ++j) {
                uint64_t a0 = a.w[i] & 0xFFFFFFFFULL, a1 = a.w[i] >> 32;
                uint64_t b0 = b.w[j] & 0xFFFFFFFFULL, b1 = b.w[j] >> 32;
                uint64_t p00 = a0 * b0;
                uint64_t p01 = a0 * b1;
                uint64_t p10 = a1 * b0;
                uint64_t p11 = a1 * b1;
                
                uint64_t mid = (p00 >> 32) + (p01 & 0xFFFFFFFFULL) + (p10 & 0xFFFFFFFFULL);
                uint64_t lo = (p00 & 0xFFFFFFFFULL) | (mid << 32);
                uint64_t hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);

                uint64_t c1 = 0, c2 = 0;
                uint64_t sum1 = res[i+j] + lo;
                c1 = (sum1 < res[i+j]) ? 1ULL : 0ULL;
                uint64_t sum2 = sum1 + carry;
                c2 = (sum2 < sum1) ? 1ULL : 0ULL;
                
                res[i+j] = sum2;
                carry = hi + c1 + c2;
            }
            res[i+4] += carry;
        }
        U256 r;
        for (int i = 0; i < 4; ++i) r.w[i] = res[i];
        return r;
    }

    // Операторы сравнения
    friend bool operator==(const U256& a, const U256& b) {
        return a.w[0] == b.w[0] && a.w[1] == b.w[1] && a.w[2] == b.w[2] && a.w[3] == b.w[3];
    }
    friend bool operator!=(const U256& a, const U256& b) { return !(a == b); }

    friend bool operator<(const U256& a, const U256& b) {
        for (int i = 3; i >= 0; --i) {
            if (a.w[i] < b.w[i]) return true;
            if (a.w[i] > b.w[i]) return false;
        }
        return false;
    }
    friend bool operator<=(const U256& a, const U256& b) { return a < b || a == b; }
    friend bool operator>(const U256& a, const U256& b) { return b < a; }
    friend bool operator>=(const U256& a, const U256& b) { return !(a < b); }

    // Constant-time побитовые операции для CSWAP
    friend U256 operator^(const U256& a, const U256& b) {
        U256 r;
        for (int i = 0; i < 4; ++i) r.w[i] = a.w[i] ^ b.w[i];
        return r;
    }
    friend U256 operator&(const U256& a, const U256& b) {
        U256 r;
        for (int i = 0; i < 4; ++i) r.w[i] = a.w[i] & b.w[i];
        return r;
    }
    friend U256 operator~(const U256& a) {
        U256 r;
        for (int i = 0; i < 4; ++i) r.w[i] = ~a.w[i];
        return r;
    }

    U256& operator^=(const U256& other) {
        for (int i = 0; i < 4; ++i) w[i] ^= other.w[i];
        return *this;
    }
};

// ============================================================================
// Модульная арифметика (глобальный модуль p)
// ============================================================================
static U256 g_p;

static U256 mod_reduce(U256 x) {
    for (int it = 0; it < 4; ++it) {
        uint64_t borrow = 0;
        U256 d;
        for (int i = 0; i < 4; ++i) {
            uint64_t d0 = x.get_word(i) - g_p.get_word(i) - borrow;
            uint64_t b1 = (x.get_word(i) < g_p.get_word(i)) ? 1ULL : 0ULL;
            uint64_t b2 = (x.get_word(i) - g_p.get_word(i) < borrow) ? 1ULL : 0ULL;
            d.set_word(i, d0);
            borrow = b1 | b2;
        }
        uint64_t mask = 0ULL - (borrow ^ 1ULL);
        U256 mask_u(mask);
        for (int i = 0; i < 4; ++i) {
            x.set_word(i, (d.get_word(i) & mask) | (x.get_word(i) & ~mask));
        }
    }
    return x;
}

static U256 mod_add(const U256& a, const U256& b) {
    U256 s = a + b;
    bool overflow = s < a;
    U256 d = s - g_p;
    bool ge = overflow || (d < s); // если overflow, то s >= p точно
    if (ge) return d;
    return s;
}

static U256 mod_sub(const U256& a, const U256& b) {
    U256 d = a - b;
    if (a < b) return d + g_p;
    return d;
}

static U256 mod_mul(const U256& a, const U256& b) {
    // Упрощение: чтобы не городить полную редукцию 512 бит в этом классе,
    // используем double-and-add по битам b (a*b mod p = sum a * 2^i mod p)
    // Это медленно, но корректно и полностью на перегруженных операторах.
    U256 res(0);
    U256 cur = mod_reduce(a);
    int n = b.bitlen();
    for (int i = 0; i < n; ++i) {
        if (b.get_bit(i)) res = mod_add(res, cur);
        cur = mod_add(cur, cur);
    }
    return res;
}

static U256 mod_pow(const U256& a, const U256& e) {
    U256 result(1);
    U256 base = mod_reduce(a);
    int n = e.bitlen();
    for (int i = 0; i < n; ++i) {
        if (e.get_bit(i)) result = mod_mul(result, base);
        base = mod_mul(base, base);
    }
    return result;
}

static U256 mod_inv(const U256& a) {
    U256 e = g_p - U256(2);
    return mod_pow(a, e);
}

// ============================================================================
// Точка на эллиптической кривой (Якоби)
// ============================================================================
struct Point {
    U256 X, Y, Z;
    Point() : X(0), Y(0), Z(0) {}
    Point(const U256& x, const U256& y, const U256& z) : X(x), Y(y), Z(z) {}
    
    bool is_infinity() const { return Z.is_zero(); }
};

static U256 g_a;
static U256 g_q;
static Point g_G;

static Point point_infinity() { return Point(U256(1), U256(1), U256(0)); }
static Point point_from_affine(const U256& x, const U256& y) { return Point(x, y, U256(1)); }

static bool point_eq_ct(const Point& a, const Point& b) {
    U256 z2a = mod_mul(a.Z, a.Z), z2b = mod_mul(b.Z, b.Z);
    U256 z3a = mod_mul(z2a, a.Z), z3b = mod_mul(z2b, b.Z);
    U256 lx = mod_mul(a.X, z2b);
    U256 rx = mod_mul(b.X, z2a);
    U256 ly = mod_mul(a.Y, z3b);
    U256 ry = mod_mul(b.Y, z3a);
    
    uint64_t acc = 0;
    for (int i = 0; i < 4; ++i) acc |= (lx.get_word(i) ^ rx.get_word(i));
    for (int i = 0; i < 4; ++i) acc |= (ly.get_word(i) ^ ry.get_word(i));
    bool xy_eq = (acc == 0);
    
    bool a_inf = a.Z.is_zero(), b_inf = b.Z.is_zero();
    uint64_t same_inf = (uint64_t)(a_inf == b_inf);
    uint64_t both_inf  = (uint64_t)(a_inf & b_inf);
    return both_inf || (same_inf && xy_eq);
}

static Point point_double(const Point& P) {
    if (P.is_infinity()) return P;
    U256 Y2 = mod_mul(P.Y, P.Y);
    U256 S  = mod_mul(mod_mul(P.X, Y2), U256(4));
    U256 Z2 = mod_mul(P.Z, P.Z);
    U256 Z4 = mod_mul(Z2, Z2);
    U256 M  = mod_add(mod_mul(U256(3), mod_mul(P.X, P.X)), mod_mul(g_a, Z4));
    U256 X3 = mod_sub(mod_mul(M, M), mod_add(S, S));
    U256 Y4 = mod_mul(Y2, Y2);
    U256 Y3 = mod_sub(mod_mul(M, mod_sub(S, X3)), mod_mul(U256(8), Y4));
    U256 Z3 = mod_mul(mod_mul(U256(2), P.Y), P.Z);
    return Point(X3, Y3, Z3);
}

static Point point_add(const Point& P1, const Point& P2) {
    if (P1.is_infinity()) return P2;
    if (P2.is_infinity()) return P1;
    U256 Z1Z1 = mod_mul(P1.Z, P1.Z);
    U256 Z2Z2 = mod_mul(P2.Z, P2.Z);
    U256 U1 = mod_mul(P1.X, Z2Z2);
    U256 U2 = mod_mul(P2.X, Z1Z1);
    U256 S1 = mod_mul(P1.Y, mod_mul(Z2Z2, P2.Z));
    U256 S2 = mod_mul(P2.Y, mod_mul(Z1Z1, P1.Z));
    U256 H  = mod_sub(U2, U1);
    U256 R  = mod_sub(S2, S1);
    if (H.is_zero()) {
        if (R.is_zero()) return point_double(P1);
        return point_infinity();
    }
    U256 H2 = mod_mul(H, H);
    U256 H3 = mod_mul(H2, H);
    U256 U1H2 = mod_mul(U1, H2);
    U256 X3 = mod_sub(mod_sub(mod_mul(R, R), H3), mod_add(U1H2, U1H2));
    U256 Y3 = mod_sub(mod_mul(R, mod_sub(U1H2, X3)), mod_mul(S1, H3));
    U256 Z3 = mod_mul(mod_mul(H, P1.Z), P2.Z);
    return Point(X3, Y3, Z3);
}

static void to_affine(const Point& P, U256& x, U256& y) {
    if (P.is_infinity()) { x = U256(0); y = U256(0); return; }
    U256 zi  = mod_inv(P.Z);
    U256 zi2 = mod_mul(zi, zi);
    U256 zi3 = mod_mul(zi2, zi);
    x = mod_mul(P.X, zi2);
    y = mod_mul(P.Y, zi3);
}

static void cswap_point(Point& a, Point& b, uint64_t bit) {
    U256 mask(0ULL - bit);
    
    U256 t = (a.X ^ b.X) & mask;
    a.X ^= t; b.X ^= t;
    t = (a.Y ^ b.Y) & mask;
    a.Y ^= t; b.Y ^= t;
    t = (a.Z ^ b.Z) & mask;
    a.Z ^= t; b.Z ^= t;
}

static Point scalar_mul_ct(const U256& k, const Point& P) {
    Point R0 = point_infinity();
    Point R1 = P;
    int n = k.bitlen();
    for (int i = n - 1; i >= 0; --i) {
        uint64_t bit = k.get_bit(i);
        cswap_point(R0, R1, bit);
        R1 = point_add(R0, R1);
        R0 = point_double(R0);
        cswap_point(R0, R1, bit);
    }
    return R0;
}

// ============================================================================
// ГОСТ Р 34.10-2012
// ============================================================================
static U256 mod_q(const U256& x) {
    U256 saved = g_p; g_p = g_q;
    U256 r = mod_reduce(x);
    g_p = saved;
    return r;
}

static U256 add_mod_q(const U256& a, const U256& b) {
    U256 saved = g_p; g_p = g_q;
    U256 r = mod_add(a, b);
    g_p = saved;
    return r;
}

static uint64_t rng_state = 0x123456789ABCDEF0ULL;
static uint64_t xorshift64() {
    uint64_t x = rng_state;
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    rng_state = x; return x;
}

static U256 random_scalar_mod_q() {
    U256 r;
    for (int i = 0; i < 4; ++i) r.set_word(i, xorshift64());
    r = mod_q(r);
    if (r.is_zero()) r = U256(1);
    return r;
}

struct Signature { U256 r, s; bool ok; };

static Signature gost_sign(const U256& e, const U256& d) {
    Signature sig; sig.ok = false;
    for (int attempt = 0; attempt < 100; ++attempt) {
        U256 k = random_scalar_mod_q();
        Point C = scalar_mul_ct(k, g_G);
        U256 xc, yc; to_affine(C, xc, yc);
        U256 r = mod_q(xc);
        if (r.is_zero()) continue;
        U256 rd = mod_q(mod_mul(r, d));
        U256 ke = mod_q(mod_mul(k, e));
        U256 s  = mod_q(add_mod_q(rd, ke));
        if (s.is_zero()) continue;
        sig.r = r; sig.s = s; sig.ok = true;
        return sig;
    }
    return sig;
}

static bool gost_verify(const U256& e, const Signature& sig, const Point& Q) {
    if (sig.r.is_zero() || sig.s.is_zero()) return false;
    if (sig.r >= g_q || sig.s >= g_q) return false;

    U256 saved = g_p; g_p = g_q;
    U256 v = mod_inv(e);
    g_p = saved;
    
    U256 z1 = mod_q(mod_mul(sig.s, v));
    U256 z2 = mod_q(mod_sub(g_q, mod_mul(sig.r, v)));
    Point C1 = scalar_mul_ct(z1, g_G);
    Point C2 = scalar_mul_ct(z2, Q);
    Point C  = point_add(C1, C2);
    U256 xc, yc; to_affine(C, xc, yc);
    U256 R = mod_q(xc);
    return R == sig.r;
}

static U256 toy_hash(const uint8_t* data, size_t len) {
    U256 h; h.set_word(0, 0xcbf29ce484222325ULL); h.set_word(1, 0x100000001b3ULL);
    for (size_t i = 0; i < len; ++i) {
        uint64_t w0 = h.get_word(0) ^ data[i];
        w0 *= 0x100000001b3ULL;
        uint64_t w1 = h.get_word(1) + (w0 ^ (w0 >> 17));
        h.set_word(0, w0);
        h.set_word(1, w1);
        h.set_word(2, (h.get_word(2) << 1) | (w1 >> 63));
    }
    return h;
}

static void init_curve() {
    g_p = U256::from_hex("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F");
    g_a = U256(0);
    g_q = U256::from_hex("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141");
    U256 gx = U256::from_hex("79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798");
    U256 gy = U256::from_hex("483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8");
    g_G = point_from_affine(gx, gy);
}

static bool point_on_curve(const U256& x, const U256& y) {
    U256 lhs = mod_mul(y, y);
    U256 rhs = mod_add(mod_add(mod_mul(mod_mul(x, x), x), mod_mul(g_a, x)), U256(7));
    return lhs == rhs;
}

int main() {
    init_curve();

    U256 gx, gy; to_affine(g_G, gx, gy);
    printf("== Параметры кривой ==\n");
    g_p.print("p");
    g_q.print("q");
    gx.print("Gx");
    gy.print("Gy");
    printf("G on curve: %s\n\n", point_on_curve(gx, gy) ? "yes" : "no");

    printf("== Constant-time сравнение точек ==\n");
    Point P1 = scalar_mul_ct(U256(1), g_G);
    Point P2 = scalar_mul_ct(U256(1), g_G);
    Point P3 = scalar_mul_ct(U256(2), g_G);
    printf("P1 == P2 : %s\n", point_eq_ct(P1, P2) ? "true" : "false");
    printf("P1 == P3 : %s\n\n", point_eq_ct(P1, P3) ? "true" : "false");

    printf("== ECDH ==\n");
    U256 dA = random_scalar_mod_q();
    U256 dB = random_scalar_mod_q();
    Point QA = scalar_mul_ct(dA, g_G);
    Point QB = scalar_mul_ct(dB, g_G);
    U256 sax, say, sbx, sby;
    to_affine(scalar_mul_ct(dA, QB), sax, say);
    to_affine(scalar_mul_ct(dB, QA), sbx, sby);
    sax.print("SharedA.x"); sbx.print("SharedB.x");
    printf("ECDH match: %s\n\n", sax == sbx ? "yes" : "no");

    printf("== ГОСТ Р 34.10-2012 ==\n");
    U256 d = random_scalar_mod_q();
    Point Q = scalar_mul_ct(d, g_G);
    const char* msg = "test message for lab3";
    U256 e = mod_q(toy_hash((const uint8_t*)msg, strlen(msg)));
    e.print("e");
    Signature sig = gost_sign(e, d);
    sig.r.print("r"); sig.s.print("s");
    printf("Verify correct: %s\n", gost_verify(e, sig, Q) ? "true" : "false");

    Signature bad = sig;
    bad.s.set_word(0, bad.s.get_word(0) ^ 1ULL);
    printf("Verify corrupted s: %s\n", gost_verify(e, bad, Q) ? "true" : "false");

    return 0;
}