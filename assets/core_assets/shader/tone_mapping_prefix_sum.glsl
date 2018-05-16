
shared uint prefix_sum[HISTOGRAM_SLOTS+1];

// based on https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch39.html
void build_prefix_sum() {
	int local_id = int(gl_LocalInvocationID.x);
	int offset = 1;

	prefix_sum[2*local_id] = local_histogram[2*local_id];
	prefix_sum[2*local_id+1] = local_histogram[2*local_id+1];

	// up-sweep phase
	for (int d = HISTOGRAM_SLOTS/2; d > 0; d /=2) {
		memoryBarrierShared();
		barrier();

		if (local_id < d) {
			uint ai = offset*(2*local_id+1)-1;
			uint bi = offset*(2*local_id+2)-1;
			prefix_sum[bi] += prefix_sum[ai];
		}
		offset *= 2;
	}

	// clear the last element
	if (local_id == 0) { prefix_sum[HISTOGRAM_SLOTS - 1] = 0; }

	// down-sweep phase
	for (int d = 1; d < HISTOGRAM_SLOTS; d *= 2) {
		offset /= 2;
		memoryBarrierShared();
		barrier();

		if (local_id < d) {
			uint ai = offset*(2*local_id+1)-1;
			uint bi = offset*(2*local_id+2)-1;
			uint t = prefix_sum[ai];
			prefix_sum[ai] = prefix_sum[bi];
			prefix_sum[bi] += t;
		}
	}

	// store the total in the last element
	if (local_id == 0) {
		prefix_sum[HISTOGRAM_SLOTS] = prefix_sum[HISTOGRAM_SLOTS-1] + histogram[HISTOGRAM_SLOTS-1];
	}
	memoryBarrierShared();
	barrier();

/* old/sequencial
	prefix_sum[0] = 0;
	prefix_sum[HISTOGRAM_SLOTS*0+1] = 0; // ignore first bucket
	for(uint i=2; i<HISTOGRAM_SLOTS+1; i++) {
		prefix_sum[i] = prefix_sum[i-1] + histogram[i-1];
	}
*/
}
