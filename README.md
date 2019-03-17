### Unicode TOFU

Check unicode strings to detect changes over time that might be spoofing.

```bash
# save string to history
echo "hi" | ./utofu my-history

# attempting to save this one causes a problem
echo "hı" | ./utofu my-history
FAILURE: string is confusable with previous value
Previous: hi
Current : hı
```

The concept is similar to how you trust SSH key signatures the first time they are used. If the signature ever changes, SSH fails. With utofu you trust strings and save them in a single-file database. Attempting to save a new string which is confusable with one already in the database causes an error.

#### How does it work?

The program relies on libicu's Unicode security and [spoofing detection](http://icu-project.org/apiref/icu4c/uspoof_8h.html). For each line from standard input, the program executes these steps:

1. Read line as UTF-8
1. Convert to [Normalization Form C](http://unicode.org/reports/tr15/#Norm_Forms) for consistency
1. Calculate skeleton string (confusable strings have the same skeleton)
1. Insert UTF-8 version of normalized input and its skeleton into the database if the skeleton doesn't already exist
1. Compare the normalized input string with the string in the database with corresponding skeleton. If not an exact match die with an error.
