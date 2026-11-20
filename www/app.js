const button = document.getElementById("test-button");
const message = document.getElementById("message");

button.addEventListener("click", function() {
    message.textContent = "Funciona";
});
