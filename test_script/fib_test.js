function fib(n) {
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}
t=now();
console.log(fib(35));
console.log(now()-t);