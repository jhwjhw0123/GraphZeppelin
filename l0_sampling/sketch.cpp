#include <math.h>
#include <cmath>
#include <assert.h>
#include <vector>
#include <iostream>
#include <string>
#include "../include/prime_generator.h"
#include "../include/sketch.h"

Sketch::Sketch(int n, long seed): n(n), seed(seed), random_prime(PrimeGenerator::generate_prime(n*n)) {
  //std::cout << "Prime: " << random_prime << std::endl;
  const int num_buckets = (log2(n)+1);
  //std::cout << "Number of buckets: " << num_buckets << std::endl;
  buckets = std::vector<Bucket>(num_buckets*(log2(n)+1));
  for (unsigned int i = 0; i < num_buckets; ++i){
    for (unsigned int j = 0; j < log2(n)+1; ++j){
      buckets[i*(log2(n)+1)+j].set_guess(1 << j);
      buckets[i*(log2(n)+1)+j].set_seed(i*(log2(n)+1)+j,seed,random_prime);
    }
  }
}

void Sketch::update(Update update ) {
  for (unsigned int j = 0; j < buckets.size(); j++){
    if (buckets[j].contains(update.index+1)){
      buckets[j].a += update.delta;
      buckets[j].b += update.delta*(update.index+1); // deals with updates whose indices are 0
      buckets[j].c += (update.delta*PrimeGenerator::power(buckets[j].r,update.index+1,random_prime))%random_prime;
      buckets[j].c = (buckets[j].c + random_prime)%random_prime;
    }
  }
}

Update Sketch::query(){
  if (already_quered){
    throw MultipleQueryException();
  }
  already_quered = true;
  for (int i = 0; i < buckets.size(); i++){
    Bucket& b = buckets[i];
    if ( b.a != 0 && b.b % b.a == 0 && (b.c - b.a*PrimeGenerator::power(b.r,b.b/b.a,random_prime))% random_prime == 0
      //Additional checks
      && 0 < b.b/b.a && b.b/b.a <= n && b.contains(b.b/b.a)){
      //cout << "Passed all tests: " << "b.a: " << b.a << " b.b: " << b.b << " b.c: " << b.c << " Guess: " << b.guess_nonzero << " r: " << b.r << endl;
      //cout << "String: " << b.stuff << endl;
      return {b.b/b.a - 1,b.a}; // 0-index adjustment
    }
  }
  throw NoGoodBucketException();
}

Sketch operator+ (const Sketch &sketch1, const Sketch &sketch2){
  assert (sketch1.n == sketch2.n);
  assert (sketch1.seed == sketch2.seed);
  assert (sketch1.random_prime == sketch2.random_prime);
  Sketch result = Sketch(sketch1.n,sketch1.seed);
  for (int i = 0; i < result.buckets.size(); i++){
    Bucket& b = result.buckets[i];
    b.a = sketch1.buckets[i].a + sketch2.buckets[i].a;
    b.b = sketch1.buckets[i].b + sketch2.buckets[i].b;
    b.c = (sketch1.buckets[i].c + sketch2.buckets[i].c)%result.random_prime;
  }
  return result;
}

Sketch operator* (const Sketch &sketch1, long scaling_factor){
  Sketch result = Sketch(sketch1.n,sketch1.seed);
  for (int i = 0; i < result.buckets.size(); i++){
    Bucket& b = result.buckets[i];
    b.a = sketch1.buckets[i].a * scaling_factor;
    b.b = sketch1.buckets[i].b * scaling_factor;
    b.c = (sketch1.buckets[i].c * scaling_factor)% result.random_prime;
  }
  return result;
}

std::ostream& operator<< (std::ostream &os, const Sketch &sketch) {
  os << sketch.random_prime << std::endl;
  for (unsigned i = 0; i < sketch.buckets.size(); i++){
    const Bucket& bucket = sketch.buckets[i];
    for (unsigned j = 0; j < sketch.n; j++) {
      os << bucket.contains(j+1) ? '1' : '0';
    }
    os << std::endl
      << "a:" << bucket.a << std::endl
      << "b:" << bucket.b << std::endl
      << "c:" << bucket.c << std::endl
      << (bucket.a != 0 && bucket.b % bucket.a == 0 && (bucket.c - bucket.a*PrimeGenerator::power(bucket.r, bucket.b/bucket.a, sketch.random_prime)) % sketch.random_prime == 0 ? "good" : "bad")
      << std::endl;
  }
  return os;
}