from cryptography.hazmat.primitives.ciphers.aead import AESGCM
import os
import secrets

key = secrets.token_bytes(32)

with open("key.h", "w") as f:
    f.write("#pragma once\n")
    f.write("constexpr unsigned char AES_KEY[32] = {")
    f.write(",".join(str(b) for b in key))
    f.write("};\n")

def encrypt_file(input_file, output_file, key_file):
    key = AESGCM.generate_key(bit_length=256)
    nonce = os.urandom(12)  # GCM standard nonce size

    aesgcm = AESGCM(key)

    with open(input_file, "rb") as f:
        plaintext = f.read()

    ciphertext = aesgcm.encrypt(nonce, plaintext, None)

    # Save [nonce || ciphertext] to file
    with open(output_file, "wb") as f:
        f.write(nonce + ciphertext)

    # Save key separately
    with open(key_file, "wb") as kf:
        kf.write(key)

    print("Encryption done.")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python3 crdtory <input.xml> <output> <key_output>")
        sys.exit(1)

    encrypt_file("credits.xml", "credits.enc", "key.bin")