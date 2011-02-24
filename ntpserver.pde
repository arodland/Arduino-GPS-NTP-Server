void setup () {
  pinMode(13, OUTPUT);
  for (int k = 0 ; k < 3 ; k++) {
    digitalWrite(13, HIGH);
    delay(250);
    digitalWrite(13, LOW);
    delay(250);
  }
}

static int i = 0;

void loop () {
  i++;
}

void second_int() {
  debug_long(time_get_ns()); debug("\n");
}
