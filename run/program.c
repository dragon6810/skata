struct inner { int x; int y; };
struct outer { struct inner i; int z; };

int main()
{
	struct outer o;

	o.i.y = 5;
	o.z = 3;

	return o.i.y + o.z;
}