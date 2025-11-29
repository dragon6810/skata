int main()
{
	int x;
	int y;

	x = 5000;
	y = 10;
	while(x)
	{
		if(y)
		{
			x = x * 2;
			y = y - 1;
		}
		x = x - 1;
	}

	return x;
}