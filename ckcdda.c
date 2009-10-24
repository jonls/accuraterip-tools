/* ckcdda.c */

/* ARCF: AccurateRip Checksum (Flawed) */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define SAMPLES_PER_FRAME                 588 /* = 44100 / 75 */
#define CHECK_RADIUS  (5*SAMPLES_PER_FRAME-1)
#define ARCFS_PER_TRACK     (2*CHECK_RADIUS+1)

#define ARCF_IDX(track,offset)  ((offset) + (track)*ARCFS_PER_TRACK)


static int
read_value(FILE *f, uint32_t *value)
{
	uint16_t sample[2];
	size_t rd = fread(sample, sizeof(uint16_t), 2, f);
	if (rd < 2) {
		if (feof(f)) return 0;
		return -1;
	}
	*value = (sample[1] << 16) | sample[0];
	return rd;
}

static void
update_arcf(uint32_t *restrict arcf, uint32_t *restrict sum, int track, int track_count,
	   const int *restrict length, int ti, int tr, int last_tr, uint32_t value)
{
	/* Update base ARCF if we're not
	   in the zone after the last track. */
	if (track < track_count) {
		/* Save first values of track in ARCF block.
		   This is the value we'll need later when
		   calculating the derived ARCFs. */
		if (tr < ARCFS_PER_TRACK-1) {
			arcf[ARCF_IDX(track, tr+1)] = value;
		}

		/* Update sum and base ARCF */
		sum[track] += value;
		arcf[ARCF_IDX(track, 0)] += value*(ti+1);
	}

	/* Calculate derived ARCFs for previous track
	   (so skip if this is the first track). */
	if (track > 0 && tr < ARCFS_PER_TRACK-1) {
		/* Fetch saved value */
		uint32_t first = arcf[ARCF_IDX(track-1, tr+1)];

		/* Calculate ARCF for moved window */
		arcf[ARCF_IDX(track-1, tr+1)] =
			arcf[ARCF_IDX(track-1, tr)] - (length[track-1]-last_tr)*first -
			sum[track-1] + length[track-1]*value;

		/* Adjust sum to be sum for new window */
		sum[track-1] += value - first;
	}
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

	uint32_t *arcf = calloc(track_count*ARCFS_PER_TRACK, sizeof(uint32_t));
	if (arcf == NULL) abort();

	int track = 0;
	printf("At track %u (%u, %u)\n", track, track < track_count, track > 0);

	int ti = CHECK_RADIUS;
	int tr = 0;
	int di = 0;

	int last_tr = 0;
	while (di < total_length) {
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

		/* Update ARCF values */
		update_arcf(arcf, sum, track, track_count, length, ti, tr, last_tr, value);

		/* Increment counters */
		di += 1;
		ti += 1;
		tr += 1;

		/* Check whether end of current track
		   has been reached. */
		if (ti == length[track]) {
			last_tr = tr;
			ti = 0;
			tr = 0;
			track += 1;
			printf("At %i track %i (%u, %u)\n", di, track, track < track_count, track > 0);
		}
	}

	/* Print ARCFs for offset 0 */
	for (int i = 0; i < track_count; i++) {
		printf("%03u,%u: %08x\n", i, 0, arcf[ARCF_IDX(i, CHECK_RADIUS)]);
	}

	return EXIT_SUCCESS;
}
