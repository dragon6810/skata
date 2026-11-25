int printf(const char* fmt, ...);

int numerator = 12;

int main(int argc, char** argv)
{
	printf("number: " "%d\n", numerator / 4);
}