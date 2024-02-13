#include    <stdint.h>

#include    "aes.h"

/* From https://en.wikipedia.org/wiki/Rijndael_S-box */
#define ROTL8(x,shift) ((uint8_t) ((x) << (shift)) | ((x) >> (8 - (shift))))

/* Adapted from https://en.wikipedia.org/wiki/Rijndael_S-box */
void initialize_aes_sbox(uint8_t sbox[256], uint8_t invsbox[256]) {
	uint8_t p = 1, q = 1;
	
	/* loop invariant: p * q == 1 in the Galois field */
	do {
		/* multiply p by 3 */
		p = p ^ (p << 1) ^ (p & 0x80 ? 0x1B : 0);

		/* divide q by 3 (equals multiplication by 0xf6) */
		q ^= q << 1;
		q ^= q << 2;
		q ^= q << 4;
		q ^= q & 0x80 ? 0x09 : 0;

		/* compute the affine transformation */
		uint8_t xformed = q ^ ROTL8(q, 1) ^ ROTL8(q, 2) ^ ROTL8(q, 3) ^ ROTL8(q, 4);

		sbox[p] = xformed ^ 0x63;
		invsbox[xformed ^ 0x63] = p;
	} while (p != 1);

	/* 0 is a special case since it has no inverse */
	sbox[0] = 0x63;
	invsbox[0x63] = 0;
}

void add_round_key(uint8_t text[16], uint8_t key[16]) {
    for (int i = 0; i < 16; i++) {
        text[i] = text[i] ^ key[i];
    }
}

void shift_rows(uint8_t text[16]) {
    for (int i = 1; i < 4; i++) {
    	uint8_t temp = text[4 * i];
    	for (int j = 0; j < 3; j++) {
    	    text[4 * i + j] = text[4 * i + j + 1];
    	}
    	text[4 * i + 3] = temp;
    }
}

void shift_rows_inv(uint8_t text[16]) {
    for (int i = 1; i < 4; i++) {
    	uint8_t temp = text[4 * i + 3];
    	for (int j = 3; j > 0; j--) {
    	    text[4 * i + j] = text[4 * i + j - 1];
    	}
    	text[4 * i] = temp;
    }
}

/* Adapted from https://en.wikipedia.org/wiki/Rijndael_MixColumns */
uint8_t g_mul(uint8_t a, uint8_t b) { // Galois Field (256) Multiplication of two Bytes
    uint8_t p = 0;

    for (int counter = 0; counter < 8; counter++) {
        if ((b & 1) != 0) {
            p = p ^ a;
        }

        int hi_bit_set = (a & 0x80) != 0;
        a = a << 1;
        if (hi_bit_set) {
            a = a ^ 0x1B; /* x^8 + x^4 + x^3 + x + 1 */
        }
        b = b >> 1;
    }

    return p;
}

void gmix_column(uint8_t column[4]) {
    uint8_t column_t[4];
    
    for (int i = 0; i < 4; i++) {
        column_t[i] = column[i];
    }
    
    column[0] = g_mul(0x02, column_t[0]) ^ g_mul(0x03, column_t[1]) ^ column_t[2] ^ column_t[3];
    column[1] = column_t[0] ^ g_mul(0x02, column_t[1]) ^ g_mul(0x03, column_t[2]) ^ column_t[3];
    column[2] = column_t[0] ^ column_t[1] ^ g_mul(0x02, column_t[2]) ^ g_mul(0x03, column_t[3]);
    column[3] = g_mul(0x03, column_t[0]) ^ column_t[1] ^ column_t[2] ^ g_mul(0x02, column_t[3]);
}

void gmix_column_inv(uint8_t column[4]) {
    uint8_t column_t[4];
    
    for (int i = 0; i < 4; i++) {
        column_t[i] = column[i];
    }
    
    column[0] = g_mul(0x0e, column_t[0]) ^ g_mul(0x0b, column_t[1]) ^ g_mul(0x0d, column_t[2]) ^ g_mul(0x09, column_t[3]);
    column[1] = g_mul(0x09, column_t[0]) ^ g_mul(0x0e, column_t[1]) ^ g_mul(0x0b, column_t[2]) ^ g_mul(0x0d, column_t[3]);
    column[2] = g_mul(0x0d, column_t[0]) ^ g_mul(0x09, column_t[1]) ^ g_mul(0x0e, column_t[2]) ^ g_mul(0x0b, column_t[3]);
    column[3] = g_mul(0x0b, column_t[0]) ^ g_mul(0x0d, column_t[1]) ^ g_mul(0x09, column_t[2]) ^ g_mul(0x0e, column_t[3]);
}

void mix_columns(uint8_t text[16]) {
    uint8_t column[4];
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            column[j] = text[4 * j + i];
        }
        
        gmix_column(column);
        
        for (int j = 0; j < 4; j++) {
            text[4 * j + i] = column[j];
        }
    }
}

void mix_columns_inv(uint8_t text[16]) {
    uint8_t column[4];
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            column[j] = text[4 * j + i];
        }
        
        gmix_column_inv(column);
        
        for (int j = 0; j < 4; j++) {
            text[4 * j + i] = column[j];
        }
    }
}

void rot_word(uint8_t key[4]) {
    uint8_t temp = key[0];
    for (int i = 0; i < 3; i++) {
        key[i] = key[i + 1];
    }
    key[3] = temp;
}

void split_word(uint8_t *to, uint32_t from) {
    for (int i = 0; i < 4; i++) {
        *(to + i) = (from >> (8 * i)) & 0xff;
    }
}

void sub_text(uint8_t text[16], uint8_t sbox[256]) {
    for (int j = 0; j < 16; j++) {
        text[j] = sbox[text[j]];
    }
}

void expkey(uint8_t rkeys[11][16], uint32_t key[4], uint8_t sbox[256]) {
    uint8_t rc[10] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36};
    uint8_t rkeys_t[11 * 16];
    uint8_t temp[4];
    
    for (int i = 0; i < 4; i++) {
        split_word(&(rkeys_t[4 * i]), key[i]);
    }
    
    for (int i = 4; i < 44; i++) {
        int prev = 4 * (i - 1);
        int prev_n = 4 * (i - 4);
        
        if (i % 4 == 0) {
            for (int j = 0; j < 4; j++) {
                temp[j] = rkeys_t[prev + j];
            }
            
            rot_word(temp);
    
            for (int j = 0; j < 4; j++) {
                rkeys_t[4 * i + j] = rkeys_t[prev_n + j] ^ sbox[temp[j]] ^ rc[(i / 4) - 1];
            }
        } else {
            for (int j = 0; j < 4; j++) {
                rkeys_t[4 * i + j] = rkeys_t[prev_n + j] ^ rkeys_t[prev + j];
            }
        }
    }
    
    for (int i = 0; i < 11; i++) {
        for (int j = 0; j < 16; j++) {
            rkeys[i][j] = rkeys_t[16 * i + j];
        }
    }
}

void encrypt(uint8_t text[16], uint8_t rkeys[11][16], uint8_t sbox[256]) {
    int rounds = 10;
    
    add_round_key(text, rkeys[0]);
    
    for (int i = 0; i < rounds - 1; i++) {
        sub_text(text, sbox);
        shift_rows(text);
        mix_columns(text);
        add_round_key(text, rkeys[i + 1]);
    }
    
    sub_text(text, sbox);
    
    shift_rows(text);
    add_round_key(text, rkeys[10]);
}

void decrypt(uint8_t text[16], uint8_t rkeys[11][16], uint8_t invsbox[256]) {
    int rounds = 10;
    
    add_round_key(text, rkeys[10]);
    
    for (int i = 0; i < rounds - 1; i++) {
        shift_rows_inv(text);
    
        sub_text(text, invsbox);
        
        add_round_key(text, rkeys[10 - i - 1]);
        mix_columns_inv(text);
    }
    
    shift_rows_inv(text);
    
    sub_text(text, invsbox);
    
    add_round_key(text, rkeys[0]);
}
