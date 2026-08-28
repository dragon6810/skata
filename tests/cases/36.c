int main()
{
	int i;
	int j;
	int c;

	c = 0;
	for(i = 0; i != 3; i = i + 1)
		for(j = 0; j != 3; j = j + 1)
			c = c + 1;

	return c;
}
