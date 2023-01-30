
int fn_4(void) { return 1; }
int fn_3(void) { return fn_4() + fn_4(); }
int fn_2(void) { return fn_4() + fn_3(); }
int fn_1(void) { return fn_4() + fn_2(); }
int main(void) { return fn_1(); }
