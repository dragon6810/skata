char *str = "ab";

int main()
{
	int i;
	int s;

	s = 0;
	for(i = 0; i != 2; i = i + 1)
		s = s + str[i];

	return s;
}
