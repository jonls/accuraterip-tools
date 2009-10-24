/* crc.c */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SAMPLES_PER_FRAME  588 /* = 44100 / 75 */
#define CHECK_RADIUS  (5*SAMPLES_PER_FRAME-1)


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

	int track_count = argc-1;

	int total_length = 0;
	int *length = calloc(track_count+1, sizeof(int));
	if (length == NULL) abort();

	for (int i = 0; i < track_count; i++) {
		length[i] = atoi(argv[i+1])*SAMPLES_PER_FRAME;
		if (i > 0) total_length += length[i];
	}

	length[track_count-1] -= CHECK_RADIUS;
	length[track_count] = CHECK_RADIUS;

	uint32_t *sum = calloc(track_count, sizeof(uint32_t));
	if (sum == NULL) abort();

	uint32_t *crc = calloc(track_count * (2*CHECK_RADIUS+1), sizeof(uint32_t));
	if (crc == NULL) abort();

	int track = 1;
	int ti = CHECK_RADIUS;
	int tr = 0;
	int di = 0;

	int last_tr = 0;
	while (di < total_length) {
		if (ti == length[track]) {
			last_tr = tr;
			ti = 0;
			tr = 0;
			track += 1;
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

		if (track < track_count) {
			/* Save first values of track in crc block */
			if (tr < 2*CHECK_RADIUS+1) {
				crc[track+(tr+1)*track_count] = value;
			}

			/* Update sum and base CRC */
			sum[track] += value;
			crc[track] += value*(ti+1);
		}

		if (track > 1 && tr < 2*CHECK_RADIUS+1) {
			/* Fetch saved value */
			uint32_t first = crc[(track-1)+(tr+1)*track_count];

			/* Calculate CRC for moved window */
			crc[(track-1)+(tr+1)*track_count] =
				crc[(track-1)+tr*track_count] - (length[track-1]-last_tr)*first -
				sum[track-1] + length[track-1]*value;

			/* Adjust sum to be sum for new window */
			sum[track-1] += value - first;
		}

		di += 1;
		ti += 1;
		tr += 1;
	}

	for (int i = 0; i < 2*CHECK_RADIUS+1; i++) {
		if (crc[i*track_count] == 0xe0547e78) printf("Offset: %i\n", i-CHECK_RADIUS);
	}

	for (int i = 0; i < track_count; i++) {
		printf("%03u,%u: %08x\n", i, 0, crc[i+CHECK_RADIUS*track_count]);
	}

	return EXIT_SUCCESS;
}
