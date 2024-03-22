class Animal {
    constructor(size) {
        this.size = size;
    }
    speak() {
        console.log("i am animal,my size is " + String(this.size));
    }
}

class Dog extends Animal {
    name = "dog";
    constructor(name, size) {
        this.name = name;
        super.constructor(size);
    };
    speak() {
        console.log("i am Dog ,my size is " + String(this.size));
    };
}
a = new Animal(5);
a.speak();
d = new Dog("dog", 6);
d.speak();
console.log(d.name);