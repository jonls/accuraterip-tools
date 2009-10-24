/* crc.c */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define SAMPLES_PER_FRAME  588 /* = 44100 / 75 */
#define CHECK_RADIUS  (5*SAMPLES_PER_FRAME-1)
#define CRCS_PER_TRACK  (2*CHECK_RADIUS+1)

static int trcnt;

static uint32_t
crc_g(const uint32_t *crc, int track, int offset)
{
	assert(track >= 0);
	assert(track < trcnt);
	assert(offset >= 0);
	assert(offset < CRCS_PER_TRACK);
	return crc[offset + track*CRCS_PER_TRACK];
}

static void
crc_s(uint32_t *crc, int track, int offset, uint32_t value)
{
	assert(track >= 0);
	assert(track < trcnt);
	assert(offset >= 0);
	assert(offset < CRCS_PER_TRACK);
	crc[offset + track*CRCS_PER_TRACK] = value;
}

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
	trcnt = track_count;
	printf("track count: %i\n", track_count);
 
	int total_length = 0;
	int *length = calloc(track_count+1, sizeof(int));
	if (length == NULL) abort();

	for (int i = 0; i < track_count; i++) {
		length[i] = atoi(argv[i+1])*SAMPLES_PER_FRAME;
		total_length += length[i];
	}
	printf("total_length: %i\n", total_length);

	length[track_count-1] -= CHECK_RADIUS+1;
	length[track_count] = 2*CHECK_RADIUS+1;

	for (int i = 0; i < track_count+1; i++) printf("len(%i): %i\n", i, length[i]);

	uint32_t *sum = calloc(track_count, sizeof(uint32_t));
	if (sum == NULL) abort();

	uint32_t *crc = calloc(track_count * CRCS_PER_TRACK, sizeof(uint32_t));
	if (crc == NULL) abort();

	int track = 0;
	printf("At track %u (%u, %u)\n", track, track < track_count, track > 0);

	int ti = CHECK_RADIUS;
	int tr = 0;
	int di = 0;

	int last_tr = 0;
	while (di < total_length) {
		if (ti == length[track]) {
			assert(track < track_count+1);
			last_tr = tr;
			ti = 0;
			tr = 0;
			track += 1;
			printf("At %i track %i (%u, %u)\n", di, track, track < track_count, track > 0);
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
			if (tr < CRCS_PER_TRACK-1) {
				crc_s(crc, track, tr+1, value);
			}

			/* Update sum and base CRC */
			sum[track] += value;
			crc_s(crc, track, 0, crc_g(crc, track, 0) + value*(ti+1));
		}

		if (track+1 > 1 && tr < CRCS_PER_TRACK-1) {
			/* Fetch saved value */
			uint32_t first = crc_g(crc, track-1, tr+1);

			/* Calculate CRC for moved window */
			crc_s(crc, track-1, tr+1,
			      crc_g(crc, track-1, tr) - (length[track-1]-last_tr)*first -
			      sum[track-1] + length[track-1]*value);

			/* Adjust sum to be sum for new window */
			sum[track-1] += value - first;
		}

		di += 1;
		ti += 1;
		tr += 1;
	}

	for (int i = 0; i < CRCS_PER_TRACK; i++) {
		if (crc[i*track_count] == 0xe0547e78) printf("Offset: %i\n", i-CHECK_RADIUS);
	}

	for (int i = 0; i < track_count; i++) {
		printf("%03u,%u: %08x\n", i, 0, crc[CHECK_RADIUS + i*CRCS_PER_TRACK]);
	}

	return EXIT_SUCCESS;
}
