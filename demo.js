class Animal {
    constructor(size) {
        this.size = size;
    };
    speak() {
        print("i am animal,my size is 2");
    };
};
class Dog extends Animal {
    name = "dog";
    constructor(name, size) {
        this.name = name;
        super.constructor(size);
    };
    speak() {
        print("i am Dog ,my size is 3");
    };
};
a = new Animal(5);
a.speak();
d = new Dog("cat", 6);
d.speak();
duck = {
    prop: a,
    speak_gua: ["gua", "gua", "gua"],
    speak_ga: ["ga", "ga", "ga"]
};
function speak(list) {
    for (i of list) {
        print(i);
    };
};
if (duck["prop"].size > 6) {
    speak(duck["speak_gua"]);
} else {
    speak(duck["speak_ga"]);
};