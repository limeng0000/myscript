class Cls extends Console{
    a = 5;
    num() {
        this.a = this.a + 1;
        return this.a;
    }
}
s = new Cls();
n = s.num();
s.log(n);