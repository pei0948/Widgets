#include <iostream>
using namespace std;

void getByteOrder()
{
				union U
				{
								short val;
								char unionByte[sizeof(short)];
				};
				U u;
				u.val=0x0102;
				if(u.unionByte[0]==1 && u.unionByte[1]==2)
				{
								cout<<"big endian"<<endl;
				}
				else if(u.unionByte[0]==2 && u.unionByte[1]==1)
				{
								cout<<"little endian"<<endl;
				}
				else
				{
								cout<<"unknown"<<endl;
				}
}

int main(int argc, char *argv[])
{
				getByteOrder();
				return 0;
}
