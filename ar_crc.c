/* crc.c */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SAMPLES_PER_FRAME  588 /* = 44100 / 75 */


static int
read_value(FILE *f, uint32_t *value)
{
	uint16_t sample[2];
	size_t rd = fread(sample, sizeof(uint16_t), 2, stdin);
	if (rd < 2) {
		if (feof(stdin)) return 0;
		return -1;
	}
	*value = (sample[1] << 16) | sample[0];
	return rd;
}

int
main(int argc, char *argv[])
{
	/* Reopen stdin as binary */
	if (freopen(NULL, "rb", stdin) == NULL) {
		perror("freopen");
		return EXIT_FAILURE;
	}

	int offset = atoi(argv[1]);

	int track_count = argc-2;
	int length[track_count];
	int total_length = 0;
	for (int i = 0; i < track_count; i++) {
		length[i] = atoi(argv[i+2])*SAMPLES_PER_FRAME;
		total_length += length[i];
	}

	length[track_count-1] -= 5*SAMPLES_PER_FRAME;

	uint32_t crc[track_count];
	memset(crc, '\0', track_count*sizeof(uint32_t));

	uint32_t crc450[track_count];
	memset(crc, '\0', track_count*sizeof(uint32_t));

	int track = 0;
	int di = 5*SAMPLES_PER_FRAME-1 + offset;
	int ti = 5*SAMPLES_PER_FRAME-1;

	/* Skip about five audio frames */
	for (int i = 0; i < di; i++) {
		uint32_t value;
		int r = read_value(stdin, &value);
		if (r == 0) {
			fprintf(stderr, "Unexpected EOF.\n");
			return EXIT_FAILURE;
		} else if (r < 0) {
			perror("read_value");	
			return EXIT_FAILURE;
		}
	}

	while (1) {
		if (ti == length[track]) {
			ti = 0;
			printf("%03u: %08x\n", track, crc[track]);
			printf("%03u: %08x (450)\n", track, crc450[track]);
			track += 1;
			if (track == track_count) break;
		}

		/* Read one stereo sample */
		uint32_t value;
		int r = read_value(stdin, &value);
		if (r == 0) {
			fprintf(stderr, "Unexpected EOF.\n");
			return EXIT_FAILURE;
		} else if (r < 0) {
			perror("read_value");
			return EXIT_FAILURE;
		}

		crc[track] += value*(ti+1);
		if (ti >= 450*SAMPLES_PER_FRAME && ti < 451*SAMPLES_PER_FRAME) {
			crc450[track] += value*(ti+1);
		}

		di += 1;
		ti += 1;
	}

	return EXIT_SUCCESS;
}
