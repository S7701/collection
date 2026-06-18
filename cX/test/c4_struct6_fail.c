struct S{int m0;} s;
int main(){
  s->m0 = 4711; // expected lhs expr type
  return 0;
}