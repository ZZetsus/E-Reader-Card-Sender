# Introduction
This is still a prototype.
It is a project to send data from Japanese e-reader+ Pokémon Colosseum Cards on physical hardware without the need to have any type of reader.
## *Note: You don't need a flashcart to run the ROM; you can use a GC/Wii program to send it via multiboot.*

### Usage
1. **Place the `.raw` files in `e-cards/raw/`** *(Recommendation: use short filenames).*
2. **Run the `Configure.py` script** located in the project root.
3. **Open a terminal in the project root and run `make`** *(You must have devKitARM installed).*


# Resources

### Video first prototype:
https://youtu.be/ARFF_fLZ07Q?si=1lyLkfLMjmceFqI5

<img width="1096" height="511" alt="image" src="https://github.com/user-attachments/assets/64d00cb5-9f6a-4e20-a4d2-566391f9b4d3" />

### Testing the first version on original hardware:
https://www.youtube.com/watch?v=AtZqVo_y_II

<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/d9025287-9c5c-4065-992e-05f393007a39" />
<img width="1920" height="1080" alt="image" src="https://github.com/user-attachments/assets/35472941-bd9e-496d-bd8d-3bd3b9ee21ff" />



# Credits
* **Decompression:** Card decompression was performed using the `nedcmake` tools created by [caitsith2](https://www.caitsith2.com/ereader/devtools.htm/).
