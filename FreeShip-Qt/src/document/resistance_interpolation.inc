// GPL-2.0-or-later. Plus SFINEX1, zero-based open arrays.
void sfinex1(int n,Span<const float> x,Span<const float> y,float x0,float &yy) {
int n1{};
int j1{};
int j2{};
int j3{};
int i{};
float sfin{};
if(n<1 || n>x.n || n>y.n) bad("Invalid interpolation size");
    sfin = 0;
    j1 = 0;
    j2 = 1;
    j3 = 2;
    n1 = n - 1;
    if (n1 == 0)
        {
            sfin = y [j1];
            goto label_exlabel;
        }
    if ((x0 <= x [0]) && (n1 > 1))
        {
            sfin = y [j1] + (y [j2] - y [j1]) * (x0 - x [j1]) / (x [j2] - x [j1]);
            goto label_exlabel;
        }
    if ((x0 > x [n1]))
        {
            sfin = y [n1] + (y [n1] - y [n1 - 1]) * (x0 - x [n1]) / (x [n1] - x [n1 - 1]);
            goto label_exlabel;
        }
    if ((x0 <= x [j2]) && (n1 >= 2))
        {
            sfin = (x0 - x [j3]) / (x [j1] - x [j2]) * ((x0 - x [j2]) / (x [j1] - x [j3]) * y [j1] - (x0 - x [j1]) / (x [j2] - x [j3]) * y [j2]) + (x0 - x [j1]) * (x0 - x [j2]) * y [j3] / ((x [j3] - x [j1]) * (x [j3] - x [j2]));
            goto label_exlabel;
        }
    if ((x0 > x [n1 - 1]))
        {
            sfin = (x0 - x [n1]) / (x [n1 - 2] - x [n1 - 1]) * ((x0 - x [n1 - 1]) / (x [n1 - 2] - x [n1]) * y [n1 - 2] - (x0 - x [n1 - 2]) * y [n1 - 1] / (x [n1 - 1] - x [n1])) + (x0 - x [n1 - 2]) * (x0 - x [n1 - 1]) / (x [n1] - x [n1 - 2]) * y [n1] / (x [n1] - x [n1 - 1]);
            goto label_exlabel;
        }
    n1 = n - 2;
    for (i=1; i<=n1; ++i)
    {
        
        if ((x0 > x [i - 1]) && (x0 <= x [i]))
            sfin = 0.5f * ((x0 - x [i - 1]) * (x0 - x [i]) * (y [i - 2] / ((x [i - 2] - x [i - 1]) * (x [i - 2] - x [i])) + y [i + 1] / ((x [i + 1] - x [i - 1]) * (x [i + 1] - x [i]))) + (x0 - x [i]) * ((x0 - x [i - 2]) / (x [i - 1] - x [i - 2]) + (x0 - x [i + 1]) / (x [i - 1] - x [i + 1])) * y [i - 1] / (x [i - 1] - x [i]) + (x0 - x [i - 1]) * ((x0 - x [i - 2]) / (x [i] - x [i - 2]) + (x0 - x [i + 1]) / (x [i] - x [i + 1])) * y [i] / (x [i] - x [i - 1]));
    }
    label_exlabel:;
    yy = sfin;
}
