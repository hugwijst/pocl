#define USE_VECTOR_DATATYPES

__kernel void
dot_product (__global const int4 *a,
        __global const int4 *b, __global int *c)
{
  size_t gid = get_global_id(0);

#ifndef USE_VECTOR_DATATYPES
  /* This version is to smoke test the autovectorization.
     Tries to create parallel regions with nice memory
     access pattern etc. so it gets autovectorizer. */
  /* This parallel region does not vectorize with the
     loop vectorizer because it accesses vector datatypes.
     Perhaps with SLP/BB vectorizer.*/

  int ax = a[gid].x;
  int ay = a[gid].y;
  int az = a[gid].z;
  int aw = a[gid].w;

  int bx = b[gid].x,
      by = b[gid].y,
      bz = b[gid].z,
      bw = b[gid].w;

  barrier(CLK_LOCAL_MEM_FENCE);

  /* This parallel region should vectorize. */
  c[gid] = ax * bx;
  c[gid] += ay * by;
  c[gid] += az * bz;
  c[gid] += aw * bw;

#else
  int4 prod = a[gid] * b[gid];
  c[gid] = prod.x + prod.y + prod.z + prod.w;
#endif


}
