# 生成光线

在世界空间中，以归一化的向量 $\mathbf{forward}$ 、 $\mathbf{up}$ 和 $\mathbf{right}$ 分别定义了相机的前方、上方和右方。

已知，相机的位置为 $\mathbf{o}$ ，视场角为大小为 $fovy$ ，成像平面在相机的正前方，与 $\mathbf{forward}$ 相互垂直，图像的宽高比为 $aspect$ ，分辨率为 $width\times height$ 。

设图像的高为 $h$ ，宽为 $w=h\times aspect$ 。那么，相机的焦距为

$$
d=\frac{h}{2\tan{\frac{fovy}{2}}}
$$

一个像素的高度为 $m=\frac{h}{height}$ ，宽度为 $n=\frac{w}{width}$ 。

图像的左下角的位置为

$$
\mathbf{s}=\mathbf{o}+\left(-\frac{w}{2}\mathbf{right}-\frac{h}{2}\mathbf{up}+d\mathbf{forward}\right)
$$

则图像左下角的像素的位置为

$$
\mathbf{s}+\frac{m}{2}\mathbf{up}+\frac{n}{2}\mathbf{right}
$$

以此类推，位于从左往右数第i列，从下往上数第j行的像素的位置为

$$
\mathbf{p}_{i,j}=\mathbf{s}+\frac{(2j+1)m}{2}\mathbf{up}+\frac{(2i+1)n}{2}\mathbf{right}
$$

注意，其中i和j都是从0开始数。然后可知，从相机位置指向该像素的位置的向量为

$$
\mathbf r=\mathbf{p}_{i,j}-\mathbf{o}\\
=d \mathbf{forward}+(\frac{(2j+1)m}{2}-\frac{h}{2})\mathbf{up}+(\frac{(2i+1)n}{2}-\frac{w}{2})\mathbf{right}\\
=\frac{h}{2}\left(\frac{1}{\tan{\frac{fovy}{2}}}\mathbf{forward}+\frac{2j+1-height}{height}\mathbf{up}+\frac{(2i+1-width)aspect}{width}\mathbf{right}\right)
$$

令 $\mathbf t=\left(\frac{1}{\tan{\frac{fovy}{2}}}\mathbf{forward}+\frac{2j+1-height}{height}\mathbf{up}+\frac{(2i+1-width)aspect}{width}\mathbf{right}\right)$ ，则光线的传播方向为

$$
\mathbf d=\frac{\mathbf t}{|\mathbf{t}|}
$$

注意， $\mathbf{d}$ 不是 $d$ 。

# 光线和三角形求交

光线为 $\mathbf{o}+t\mathbf{d}$，其中$\mathbf{o}$ 为光的发射点， $\mathbf{d}$ 为光的传播方向。

三角形的三点分别为A、B和C，向量 $\mathbf{a}=\mathbf{CB}$，$\mathbf{b}=\mathbf{CA}$ 。三角形内任意一点可 $\mathbf{p}$ 由以下式子表示。

$$
\mathbf{p}=\mathbf{C}+\alpha \mathbf{a}+\beta\mathbf{b}
$$

$$
\alpha >0, \beta>0, \alpha+\beta<1
$$

求三角形与光线是否相交以及交点，只需解方程

$$
\mathbf{o}+t\mathbf{d}=\mathbf{C}+\alpha \mathbf{a}+\beta\mathbf{b}
$$

$$
\alpha >0, \beta>0, \alpha+\beta<1, t>0
$$

假设 $\mathbf a=(a_x,a_y,a_z)$ ， $\mathbf{b}=(b_x,b_y,b_z)$ ， $\mathbf{t}=(t_x,t_y,t_z)$ ， $\mathbf{o}=(o_x,o_y,o_z)$ ， $\mathbf{C}=(c_x,c_y,c_z)$ ，则有方程

$$
\left[
\begin{matrix}
a_x&b_x&d_x \\
a_y&b_y&d_y \\
a_z&b_z&d_z
\end{matrix}
\right]
\begin{bmatrix}
\alpha \\
\beta \\
-t
\end{bmatrix}
=\begin{bmatrix}
o_x - c_x \\
o_y - c_y \\
o_z - c_z
\end{bmatrix}
$$

Cramer's Law可解上式。

# 光线与轴对齐包围盒求交
包围盒由6个平面所围成，这6个平面为 $X_1$ ， $X_2$ ， $Y_1$ ， $Y_2$ ， $Z_1$ ， $Z_2$ ，其中， $X_1$ 和 $X_2$ 平行， $Y_1$ 和 $Y_2$ 平行， $Z_1$ 和 $Z_2$ 平行。光线为 $\mathbf{o}+t\mathbf{d}$ 。

当光线和这3对平面都相交时，交点的参数 $t$ 依次分别为 $t_x^1$ 、 $t_x^2$ 、 $t_y^1$ 、 $t_y^2$ 、 $t_z^1$ 、 $t_z^2$ 。如果光线与包围盒相交，那么必然存在参数 $t$ 满足以下条件。

$$
t\in (t_x^1,t_x^2),t\in (t_y^1,t_y^2),t\in (t_z^1,t_z^2),t\in(0,+\infty)
$$

也就是说， $(t_x^1,t_x^2)$ 、 $(t_y^1,t_y^2)$ 、 $(t_z^1,t_z^2)$ 和 $(0,+\infty)$ 有交集。

当光线只与2对或1对平面相交时，这种情况是上述情况的退化，不言自明。

# 立体角均匀采样
在立体角元 $\mathrm d\Omega$ 内的概率

$$
\mathrm dP=\frac{\mathrm d\Omega}{4\pi}=\frac{\sin\theta}{4\pi}\,\mathrm d\theta\,\mathrm d\varphi=-\frac{1}{4\pi}\mathrm d\cos\theta\,\mathrm d\varphi
$$

因此只需要 $\cos\theta$ 和 $\varphi$ 分别是均匀分布即可。

# Rendering Equation 渲染方程
## Radiant flux 辐射通量
单位时间内通过某一截面的辐射能量，也可以说是通过某一表面的辐射功率，单位是瓦特（ $W$ ）。

$$
\Phi=\frac{\mathrm{d}Q}{\mathrm{d}t}
$$

## Radiant flux density 辐射通量密度
单位面积上的辐射功率，单位是瓦特每平方米（ $W \cdot m^{-2}$ ）。

$$
E=\frac{\mathrm{d}\Phi}{\mathrm{d}A}
$$

## Radiance 辐射率
单位立体角上的辐射通量密度，单位是 $W\cdot m^{-2}\cdot sr^{-1}$ 。

$$
L=\frac{\mathrm{d}E}{\mathrm{d}\omega}
=\frac{\mathrm{d}\Phi}{\mathrm{d}A\mathrm{d}\omega}
$$

## BRDF 双向反射分布函数
BRDF的全称是Bi-directional Reflection Distribution Function，它被定义为沿出射光方向 $\omega_o$ 出射的辐射率 $L$ ，与沿入射光方向 $\omega_i$ 入射的辐照度 $E$ 的比值，单位为 $sr^{-1}$ 。由上述表述，则在物体表面某处的BRDF的定义式即为

$$
f_r(\omega_i\rightarrow\omega_o)=\frac{\mathrm{d}L(\omega_o)}{\mathrm{d}E(\omega_i)}
$$

## 渲染方程的推导
由BRDF的定义可得

$$
\mathrm{d}L(\omega_o)=f_r(\omega_i\rightarrow\omega_o)\mathrm{d}E(\omega_i)
$$

因此

$$
L(\omega_o)=\int{\mathrm{d}L(\omega_o)}
=\int{f_r(\omega_i\rightarrow\omega_o)\mathrm{d}E(\omega_i)}
$$

又因为这里的 $\mathrm{d}E$ 是垂直于表面的，所以

$$
\mathrm{d}E(\omega_i)=L(\omega_i)\mathrm{d}\omega_i\cos{\theta}
$$

其中 $\theta$ 是入射角。因此

$$
L(\omega_o)=\int{\mathrm{d}L(\omega_o)}
=\int{f_r(\omega_i\rightarrow\omega_o)L(\omega_i)\cos{\theta}\mathrm{d}\omega_i}
$$

# 俄罗斯轮盘赌博的数学期望

设每一回合的死亡概率为 $p$，并且 $p\neq 0$ 且 $p \neq 1$，那么发生死亡的回合数符合概率分布 $X$ ， $P(X=i)=p(1-p)^{i-1}$ 。也就是说，死于第 $i$ 回合的概率为 $p(1-p)^{i-1}$。

设

$$
S_n=\sum_{i=1}^{n}{i(1-p)^{i-1}p}
$$

那么

$$
E(X)=\lim_{n\rightarrow+\infty}{S_n}
$$

使用错位相减法，因为

$$
S_n=p+2(1-p)p+3(1-p)^{2}p+...+n(1-p)^{n-1}p
$$

$$
(1-p)S_n=(1-p)p+2(1-p)^{2}p+3(1-p)^{3}p+...+n(1-p)^{n}p
$$

所以,两式相减，得

$$
pS_n
=p+(1-p)p+(1-p)^{2}p+...+(1-p)^{n-1}p-n(1-p)^{n}p
=p\left(\sum_{i=1}^{n}{(1-p)^{i-1}}\right)-n(1-p)^{n}p
$$

$$
S_n
=\left(\sum_{i=1}^{n}{(1-p)^{i-1}}\right)-n(1-p)^{n}
=\frac{1-(1-p)^n}{p}-n(1-p)^n
$$

所以

$$
E(X)
=\lim_{n\rightarrow+\infty}{\left[\frac{1-(1-p)^n}{p}-n(1-p)^n\right]}
=\frac{1}{p}
$$
