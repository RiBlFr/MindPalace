const screens = new Map(
  [...document.querySelectorAll(".screen")].map((screen) => [screen.dataset.screen, screen])
);

function showScreen(name) {
  screens.forEach((screen, key) => {
    screen.classList.toggle("is-active", key === name);
  });
}

document.addEventListener("click", (event) => {
  const routeButton = event.target.closest("[data-route]");
  if (routeButton) {
    showScreen(routeButton.dataset.route);
    return;
  }

  const feedbackButton = event.target.closest("[data-feedback]");
  if (!feedbackButton) return;

  feedbackButton.classList.remove("is-pulsing");
  void feedbackButton.offsetWidth;
  feedbackButton.classList.add("is-pulsing");
});
