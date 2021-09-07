#include "../include/sketch.h"
#include <cassert>
#include <iostream>

Sketch::SketchUniquePtr Sketch::makeSketch(const Sketch &old) {
  return makeSketch(old.n, old.seed, old.num_bucket_factor);
}

Sketch::SketchUniquePtr Sketch::makeSketch(vec_t n, long seed, double num_bucket_factor) {
  void* loc = malloc(sketchSizeof(n, num_bucket_factor));
  return SketchUniquePtr(makeSketch(loc, n, seed, num_bucket_factor), [](Sketch* s){ s->~Sketch(); free(s); });
}

Sketch::SketchUniquePtr Sketch::makeSketch(vec_t n, long seed, std::fstream &binary_in) {
  double num_bucket_factor = 0.0;
  binary_in.read((char*)&num_bucket_factor, sizeof(double));
  void* loc = malloc(sketchSizeof(n, num_bucket_factor));
  return SketchUniquePtr(makeSketch(loc, n, seed, num_bucket_factor, binary_in), [](Sketch* s){ free(s); });
}

Sketch* Sketch::makeSketch(void* loc, vec_t n, long seed, std::fstream &binary_in) {
  double num_bucket_factor = 0.0;
  binary_in.read((char*)&num_bucket_factor, sizeof(double));
  return makeSketch(loc, n, seed, num_bucket_factor, binary_in);
}

Sketch* Sketch::makeSketch(void* loc, vec_t n, long seed, double num_bucket_factor) {
  return new (loc) Sketch(n, seed, num_bucket_factor);
}

Sketch* Sketch::makeSketch(void* loc, vec_t n, long seed, double num_bucket_factor, std::fstream &binary_in) {
  return new (loc) Sketch(n, seed, num_bucket_factor, binary_in);
}

Sketch::Sketch(vec_t n, long seed, double num_bucket_factor):
    seed(seed), n(n), num_bucket_factor(num_bucket_factor) {
  for (size_t i = 0; i < get_num_elems(); ++i) {
    get_bucket_a()[i] = 0;
    get_bucket_c()[i] = 0;
  }
}

Sketch::Sketch(vec_t n, long seed, double num_bucket_factor, std::fstream &binary_in):
    seed(seed), n(n), num_bucket_factor(num_bucket_factor) {
  binary_in.read((char*)get_bucket_a(), get_num_elems() * sizeof(vec_t));
  binary_in.read((char*)get_bucket_c(), get_num_elems() * sizeof(vec_hash_t));
}

void Sketch::update(const vec_t& update_idx) {
  const unsigned num_buckets = bucket_gen(n, num_bucket_factor);
  const unsigned num_guesses = guess_gen(n);
  XXH64_hash_t update_hash = Bucket_Boruvka::index_hash(update_idx, seed);
  for (unsigned i = 0; i < num_buckets; ++i) {
    col_hash_t col_index_hash = Bucket_Boruvka::col_index_hash(i, update_idx, seed);
    for (unsigned j = 0; j < num_guesses; ++j) {
      unsigned bucket_id = i * num_guesses + j;
      if (Bucket_Boruvka::contains(col_index_hash, 1 << j)) {
        Bucket_Boruvka::update(get_bucket_a()[bucket_id], get_bucket_c()[bucket_id], update_idx, update_hash);
      } else break;
    }
  }
}

void Sketch::batch_update(const std::vector<vec_t>& updates) {
  for (const auto& update_idx : updates) {
    update(update_idx);
  }
}

vec_t Sketch::query() {
  if (already_quered) {
    throw MultipleQueryException();
  }
  already_quered = true;
  bool all_buckets_zero = true;
  const unsigned num_buckets = bucket_gen(n, num_bucket_factor);
  const unsigned num_guesses = guess_gen(n);
  for (unsigned i = 0; i < num_buckets; ++i) {
    for (unsigned j = 0; j < num_guesses; ++j) {
      unsigned bucket_id = i * num_guesses + j;
      if (get_bucket_a()[bucket_id] != 0 || get_bucket_c()[bucket_id] != 0) {
        all_buckets_zero = false;
      }
      if (Bucket_Boruvka::is_good(get_bucket_a()[bucket_id], get_bucket_c()[bucket_id], n, i, 1 << j, seed)) {
        return get_bucket_a()[bucket_id];
      }
    }
  }
  if (all_buckets_zero) {
    throw AllBucketsZeroException();
  } else {
    throw NoGoodBucketException();
  }
}

Sketch &operator+= (Sketch &sketch1, const Sketch &sketch2) {
  assert (sketch1.n == sketch2.n);
  assert (sketch1.seed == sketch2.seed);
  assert (sketch1.num_bucket_factor == sketch2.num_bucket_factor);
  for (unsigned i = 0; i < sketch1.get_num_elems(); i++){
    sketch1.get_bucket_a()[i] ^= sketch2.get_bucket_a()[i];
    sketch1.get_bucket_c()[i] ^= sketch2.get_bucket_c()[i];
  }
  sketch1.already_quered = sketch1.already_quered || sketch2.already_quered;
  return sketch1;
}

bool operator== (const Sketch &sketch1, const Sketch &sketch2) {
  if (sketch1.n != sketch2.n ||
      sketch1.seed != sketch2.seed ||
      sketch1.num_bucket_factor != sketch2.num_bucket_factor ||
      sketch1.already_quered != sketch2.already_quered) return false;

  for (size_t i = 0; i < sketch1.get_num_elems(); ++i)
  {
    if (sketch1.get_bucket_a()[i] != sketch2.get_bucket_a()[i]) return false;
  }

  for (size_t i = 0; i < sketch1.get_num_elems(); ++i)
  {
    if (sketch1.get_bucket_c()[i] != sketch2.get_bucket_c()[i]) return false;
  }

  return true;
}

std::ostream& operator<< (std::ostream &os, const Sketch &sketch) {
  const unsigned long long int num_buckets = bucket_gen(sketch.n, sketch.num_bucket_factor);
  const unsigned long long int num_guesses = guess_gen(sketch.n);
  for (unsigned i = 0; i < num_buckets; ++i) {
    for (unsigned j = 0; j < num_guesses; ++j) {
      unsigned bucket_id = i * num_guesses + j;
      for (unsigned k = 0; k < sketch.n; k++) {
        os << (Bucket_Boruvka::contains(Bucket_Boruvka::col_index_hash(i, k, sketch.seed), 1 << j) ? '1' : '0');
      }
      os << std::endl
         << "a:" << sketch.get_bucket_a()[bucket_id] << std::endl
         << "c:" << sketch.get_bucket_c()[bucket_id] << std::endl
         << (Bucket_Boruvka::is_good(sketch.get_bucket_a()[bucket_id], sketch.get_bucket_c()[bucket_id], sketch.n, i, 1 << j, sketch.seed) ? "good" : "bad") << std::endl;
    }
  }
  return os;
}

void Sketch::write_binary(std::fstream& binary_out) {
  binary_out.write((char*)&num_bucket_factor, sizeof(double));
  binary_out.write((char*)get_bucket_a(), get_num_elems()*sizeof(vec_t));
  binary_out.write((char*)get_bucket_c(), get_num_elems()*sizeof(vec_hash_t));
}
