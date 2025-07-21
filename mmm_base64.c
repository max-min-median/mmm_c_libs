#include <stdio.h>
#include <stdint.h>

int base64_encode(char *in, char *out) {
    
    FILE *in_file = fopen(in, "r");
    if (in_file == NULL) { printf("Error opening in-file '%s'", in); return 1; }
    FILE *out_file = fopen(out, "w");
    if (out_file == NULL) { printf("Error opening out-file '%s'", out); return 1; }

    char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char padding = '=';

    while (!feof(in_file)) {
        uint8_t three[3];
        uint32_t full;
        uint8_t to_write[4];
        int bytesread = fread(three, 1, 3, in_file);
        if (bytesread == 3) {
            full = (three[0] << 16) + (three[1] << 8) + three[2];
        } else if (bytesread == 1) {
            full = (three[0] << 4);
            to_write[3] = to_write[2] = padding;
        } else if (bytesread == 2) {
            full = (three[0] << 18) + (three[1] << 10);
            to_write[3] = padding;
        } else if (bytesread == 0) {
            // should check for error if not feof
            continue;
        }

        for ( ; bytesread >= 0; bytesread--) {  // bytesread happens to equal the position of to_write to start writing to, so use it.
            to_write[bytesread] = alphabet[full & 0b111111];
            full >>= 6;
        }
        fwrite(to_write, 1, 4, out_file);
    }

    fclose(in_file);
    fclose(out_file);
}

int main(void) {
    base64_encode("test_file_200_bytes.txt", "outfile.txt");
}