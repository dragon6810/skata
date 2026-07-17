struct inner { char x; short y; };
struct outer { struct inner i; long long z; };

int main()
{
	struct outer o;
	struct inner i;

	i.y = 5;

	o.i = i;
	o.z = 3;

	return o.i.y + o.z;
}
