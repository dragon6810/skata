int main()
{
	long long x;
	short y;
	long long *p;

	y = 2;
	x = 1;
	while(y)
	{
		y = y + 1;
		x = x + 1;
	}

	p = &x;
	*p = *p + 1;

	return x == *p;
}
