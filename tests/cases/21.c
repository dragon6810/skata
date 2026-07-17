struct sum { int x; int y; };
struct res { int a; };

struct res add(struct sum nums)
{
	struct res result;

	result.a = nums.x + nums.y;

	return result;
}

int main()
{
	struct sum input;

	input.x = 1;
	input.y = 2;
	return add(input).a;
}
