#include "stdio.h"

int a(){
	int a = 1;
	int b = 2;
	return a+b;
}

int b(){
	int a = 10;
	return a;
}

int main(){
	a();
	b();
	return 0;
}