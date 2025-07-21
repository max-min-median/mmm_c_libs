#ifndef MMM_FFGETS
#define MMM_FFGETS

#include <stdio.h>
#include <stdint.h>

/**
 * Reads a line of up to `_MaxCount` characters from `_File`, terminating just before CRLF (or LF) is reached, and stores
 * that into `_Buf` as a null-terminated string. The user is responsible for ensuring that the size of `_Buf` is at least
 * `_MaxCount + 1` bytes.
 * 
 * @return
 * - `_MaxCount`, if the line was completely read.
 * - `_MaxCount + 1`, if there are still characters before the next CRLF (or LF).
 * - -1, if EOF encountered at the start of a read.
 * - -2, if there is an error.
 */
int64_t ffgets(char *__restrict__ _Buf, uint32_t _MaxCount, FILE *__restrict__ _File) {

    int64_t bytesread;
    for (bytesread = 0; bytesread < _MaxCount + 1; bytesread++) {
        int ch = fgetc(_File);
        if (ch == EOF) {
            if (feof(_File)) {
                _Buf[bytesread] = '\0';
                return bytesread == 0 ? -1 : bytesread;
            } else { // stream error
                printf("Stream error!\n");
                return -2;
            }
        }
        if (ch == '\n') {
            _Buf[bytesread] = '\0';
            return bytesread;
        }
        _Buf[bytesread] = ch;
    }
    // At this point, (_MaxCount + 1) bytes have been read without encountering LF.
    // Stuff back the last byte and return (_MaxCount + 1);
    ungetc(_Buf[bytesread - 1], _File);
    _Buf[bytesread - 1] = '\0';
    return bytesread;
}

#endif // MMM_FFGETS