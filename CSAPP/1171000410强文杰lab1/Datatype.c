#include <stdio.h>
typedef unsigned char *byte_pointer;
struct HAHA{
int a,b;
};
union LALA{
int x,y;
};
void show_bytes(byte_pointer start,size_t len)
{
    for(size_t i=0;i<len;i++)
    {
        printf("%.2x ",start[i]);
    }
    printf("\n");
}
void show_int(int x)
{
    printf("int %d  %x  ",x,&x);
    show_bytes((byte_pointer)&x,sizeof(int));
}
void show_float(float x)
{
    printf("float %f  %x  ",x,&x);
    show_bytes((byte_pointer)&x,sizeof(float));
}
void show_char(char x)
{
    printf("char %c  %x  ",x,&x);
    show_bytes((byte_pointer)&x,sizeof(char));
}
void show_struct(struct HAHA x)
{
    printf("struct  HAHA  %d  %d  %x  ",x.a,x.b,&x);
    show_bytes((byte_pointer)&x,sizeof(struct HAHA));
}
void show_point(void *x)
{
    printf("point  %x  ",x);
    show_bytes((byte_pointer)&x,sizeof(void*));
}
void show_array(int x[],int n)
{
    printf("array");
    for(int i=0;i<n;i++)
    {
        printf("%d  ",x[i]);
    }
    printf("%x  ",x);
    show_bytes((byte_pointer)x,n*sizeof(int));
}
void show_union(union LALA x)
{
    printf("union LALA  %d  %d  %x  %x  ",x.x,x.y,&x.x,&x.y);
    show_bytes((byte_pointer)&x,sizeof(union LALA));
}
int a1=12345;
float a2=12345.0;
int main()
{
    int b[3];
    b[0]=123,b[1]=456,b[2]=789;
    char c='a';
    struct HAHA m;
    m.a=1,m.b=1;
    int *p=&a1;
    union LALA n;
    n.x=222;
    n.y=333;
    enum HA{fir=100,sec=200};
    enum HA q=fir;
    show_int(a1);
    show_float(a2);
    show_char(c);
    show_struct(m);
    show_point(p);
    show_array(b,3);
    show_union(n);
    printf("enum HA q %d %x",q,&q);
    show_bytes((byte_pointer)&q,sizeof(enum HA));
    printf("printf %x\n",printf);
    printf("main %x\n",main);
    return 0;
}
