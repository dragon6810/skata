int main()
{
	long long x;
	short y;

	y = 2;
	x = 1;
	while(y)
	{
		y = y + 1;
		x = x + 1;
	}

	return &x;
}