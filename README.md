# mrb

C++ binder library for mruby

Basic usage;

```c++
class Game
{
public:
    Game() = default;
    void run(int frames) {}
};
```
```c++
    // You use the default mruby creation function
    auto* ruby = mrb_open();
    // Expects `Game` to have a parameterless constructor
    mrb::make_class<Game>(ruby, "Game");
    mrb::add_method<&Game::run>(ruby, "run");
```

```ruby
    game = Game.new()
    game.run(50)
```

### API

#### make_class
```c++
RClass* mrb::make_class<CLASS>(mrb_state* ruby, const char* name, RClass* parent = nullptr)
    
RClass* mrb::make_noinit_class<CLASS>(mrb_state* ruby, const char* name, RClass* parent = nullptr)
```

Expose a C++ class to ruby. First version must have a parameterless constructor. Second version can not be called with `new` from ruby.

#### add_method
```c++

mrb::add_method<&Method>(mrb_state* ruby, const char* name);

mrb::add_method<Class>(mrb_state* ruby, const char* name, Function fn);
```
Expose a method in a registered class.
```c++
add_method<Game>(ruby, "add_score", [](Game* game, int score) {
    game->score += score;
});
```

#### accessors
```c++
mrb::attr_accessor<&Field>(mrb_state* ruby, const char* name);

mrb::attr_reader<&Field>(mrb_state* ruby, const char* name);

mrb::attr_writer<&Field>(mrb_state* ruby, const char* name);
```

Expose a field in a registered class.

```c++
mrb::attr_reader<&Game::score>(ruby, "score");
```