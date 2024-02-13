#ifndef AES_HEADER
#define AES_HEADER

void initialize_aes_sbox(uint8_t[256], uint8_t[256]);
void shift_rows(uint8_t[16]);
void shift_rows_inv(uint8_t[16]);
void mix_columns(uint8_t[16]);
void mix_columns_inv(uint8_t[16]);
void sub_text(uint8_t[16], uint8_t[256]);
void expkey(uint8_t[11][16], uint32_t[4], uint8_t[256]);
void encrypt(uint8_t[16], uint8_t[11][16], uint8_t[256]);
void decrypt(uint8_t[16], uint8_t[11][16], uint8_t[256]);

#endif
